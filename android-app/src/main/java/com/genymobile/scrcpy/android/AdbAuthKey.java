package com.genymobile.scrcpy.android;

import android.content.Context;
import android.content.SharedPreferences;
import android.security.keystore.KeyGenParameterSpec;
import android.security.keystore.KeyProperties;
import android.util.Base64;

import java.io.IOException;
import java.math.BigInteger;
import java.security.KeyFactory;
import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.KeyStore;
import java.security.PrivateKey;
import java.security.GeneralSecurityException;
import java.security.interfaces.RSAPublicKey;
import java.security.spec.PKCS8EncodedKeySpec;
import java.security.spec.X509EncodedKeySpec;

import javax.crypto.Cipher;
import javax.crypto.KeyGenerator;
import javax.crypto.SecretKey;
import javax.crypto.spec.GCMParameterSpec;

final class AdbAuthKey {
    private static final String PREFS = "adb-auth";
    private static final String PRIVATE = "private";
    private static final String PUBLIC = "public";
    private static final String ENCRYPTED_PRIVATE = "private-encrypted";
    private static final String ENCRYPTED_PUBLIC = "public-encrypted";
    private static final String KEYSTORE = "AndroidKeyStore";
    private static final String WRAPPING_KEY_ALIAS = "scrcpy-adb-auth-wrapping";
    private static final int GCM_IV_LENGTH = 12;
    private static final int GCM_TAG_LENGTH_BITS = 128;
    private static final int RSA_WORDS = 64;
    private static final byte[] SHA1_DIGEST_INFO_PREFIX = new byte[]{
            0x30, 0x21, 0x30, 0x09, 0x06, 0x05, 0x2b, 0x0e,
            0x03, 0x02, 0x1a, 0x05, 0x00, 0x04, 0x14
    };
    private final PrivateKey privateKey;
    private final RSAPublicKey publicKey;

    AdbAuthKey(Context context) throws IOException {
        try {
            SharedPreferences preferences = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
            KeyFactory factory = KeyFactory.getInstance("RSA");
            String encryptedPrivate = preferences.getString(ENCRYPTED_PRIVATE, null);
            String encryptedPublic = preferences.getString(ENCRYPTED_PUBLIC, null);
            String privateEncoded = preferences.getString(PRIVATE, null);
            String publicEncoded = preferences.getString(PUBLIC, null);

            if ((encryptedPrivate == null) != (encryptedPublic == null)) {
                throw new IOException("The stored ADB authentication key is incomplete");
            }

            PrivateKey loadedPrivate;
            RSAPublicKey loadedPublic;
            if (encryptedPrivate != null) {
                loadedPrivate = factory.generatePrivate(new PKCS8EncodedKeySpec(
                        decrypt(encryptedPrivate)));
                loadedPublic = (RSAPublicKey) factory.generatePublic(new X509EncodedKeySpec(
                        decrypt(encryptedPublic)));
            } else if (privateEncoded != null && publicEncoded != null) {
                loadedPrivate = factory.generatePrivate(new PKCS8EncodedKeySpec(
                        Base64.decode(privateEncoded, Base64.DEFAULT)));
                loadedPublic = (RSAPublicKey) factory.generatePublic(new X509EncodedKeySpec(
                        Base64.decode(publicEncoded, Base64.DEFAULT)));
                saveEncryptedKey(preferences, loadedPrivate, loadedPublic);
            } else {
                KeyPairGenerator generator = KeyPairGenerator.getInstance("RSA");
                generator.initialize(2048);
                KeyPair pair = generator.generateKeyPair();
                loadedPrivate = pair.getPrivate();
                loadedPublic = (RSAPublicKey) pair.getPublic();
                saveKey(preferences, loadedPrivate, loadedPublic);
            }
            privateKey = loadedPrivate;
            publicKey = loadedPublic;
        } catch (Exception e) {
            throw new IOException("Could not create the ADB authentication key", e);
        }
    }

    static void reset(Context context) throws IOException {
        SharedPreferences preferences = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        try {
            KeyStore keyStore = KeyStore.getInstance(KEYSTORE);
            keyStore.load(null);
            AdbAuthKeyReset.reset(new AdbAuthKeyReset.Operations() {
                @Override
                public void deleteWrappingKey() throws Exception {
                    if (keyStore.containsAlias(WRAPPING_KEY_ALIAS)) {
                        keyStore.deleteEntry(WRAPPING_KEY_ALIAS);
                    }
                }

                @Override
                public boolean clearPreferences() {
                    return preferences.edit().clear().commit();
                }
            });
        } catch (IOException error) {
            throw error;
        } catch (Exception error) {
            throw new IOException("Could not reset the ADB authorization key", error);
        }
    }

    private static void saveKey(SharedPreferences preferences,
            PrivateKey privateKey, RSAPublicKey publicKey
    ) throws GeneralSecurityException, IOException {
        saveEncryptedKey(preferences, privateKey, publicKey);
    }

    private static void saveEncryptedKey(SharedPreferences preferences,
            PrivateKey privateKey, RSAPublicKey publicKey
    ) throws GeneralSecurityException, IOException {
        preferences.edit()
                .putString(ENCRYPTED_PRIVATE, encrypt(privateKey.getEncoded()))
                .putString(ENCRYPTED_PUBLIC, encrypt(publicKey.getEncoded()))
                .remove(PRIVATE)
                .remove(PUBLIC)
                .apply();
    }

    private static SecretKey wrappingKey() throws GeneralSecurityException, IOException {
        KeyStore keyStore = KeyStore.getInstance(KEYSTORE);
        keyStore.load(null);
        java.security.Key existing = keyStore.getKey(WRAPPING_KEY_ALIAS, null);
        if (existing instanceof SecretKey) {
            return (SecretKey) existing;
        }
        KeyGenerator generator = KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, KEYSTORE);
        generator.init(new KeyGenParameterSpec.Builder(WRAPPING_KEY_ALIAS,
                KeyProperties.PURPOSE_ENCRYPT | KeyProperties.PURPOSE_DECRYPT)
                .setBlockModes(KeyProperties.BLOCK_MODE_GCM)
                .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
                .build());
        return generator.generateKey();
    }

    private static String encrypt(byte[] plaintext) throws GeneralSecurityException, IOException {
        Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
        cipher.init(Cipher.ENCRYPT_MODE, wrappingKey());
        byte[] iv = cipher.getIV();
        if (iv == null || iv.length != GCM_IV_LENGTH) {
            throw new IOException("The Android Keystore returned an invalid AES-GCM IV");
        }
        byte[] ciphertext = cipher.doFinal(plaintext);
        byte[] encoded = new byte[iv.length + ciphertext.length];
        System.arraycopy(iv, 0, encoded, 0, iv.length);
        System.arraycopy(ciphertext, 0, encoded, iv.length, ciphertext.length);
        return Base64.encodeToString(encoded, Base64.NO_WRAP);
    }

    private static byte[] decrypt(String encoded) throws GeneralSecurityException, IOException {
        byte[] data = Base64.decode(encoded, Base64.DEFAULT);
        if (data.length <= GCM_IV_LENGTH) {
            throw new IOException("The stored ADB authentication key is invalid");
        }
        byte[] iv = new byte[GCM_IV_LENGTH];
        byte[] ciphertext = new byte[data.length - iv.length];
        System.arraycopy(data, 0, iv, 0, iv.length);
        System.arraycopy(data, iv.length, ciphertext, 0, ciphertext.length);
        Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
        cipher.init(Cipher.DECRYPT_MODE, wrappingKey(), new GCMParameterSpec(GCM_TAG_LENGTH_BITS, iv));
        return cipher.doFinal(ciphertext);
    }

    byte[] sign(byte[] token) throws IOException {
        try {
            if (token.length != 20) {
                throw new IOException("Unexpected ADB challenge length: " + token.length);
            }
            byte[] digestInfo = new byte[SHA1_DIGEST_INFO_PREFIX.length + token.length];
            System.arraycopy(SHA1_DIGEST_INFO_PREFIX, 0, digestInfo, 0,
                    SHA1_DIGEST_INFO_PREFIX.length);
            System.arraycopy(token, 0, digestInfo, SHA1_DIGEST_INFO_PREFIX.length,
                    token.length);
            Cipher cipher = Cipher.getInstance("RSA/ECB/PKCS1Padding");
            cipher.init(Cipher.ENCRYPT_MODE, privateKey);
            return cipher.doFinal(digestInfo);
        } catch (Exception e) {
            throw new IOException("Could not sign the ADB authentication token", e);
        }
    }

    byte[] publicKeyPayload() {
        byte[] blob = publicKeyBlob();
        String encoded = Base64.encodeToString(blob, Base64.NO_WRAP)
                + " controller@android\0";
        return encoded.getBytes(java.nio.charset.StandardCharsets.UTF_8);
    }

    private byte[] publicKeyBlob() {
        byte[] blob = new byte[4 + 4 + RSA_WORDS * 4 + RSA_WORDS * 4 + 4];
        int offset = 0;
        writeIntLE(blob, offset, RSA_WORDS);
        offset += 4;
        int[] modulus = toWords(publicKey.getModulus());
        writeIntLE(blob, offset, montgomeryInverse(modulus[0]));
        offset += 4;
        for (int word : modulus) {
            writeIntLE(blob, offset, word);
            offset += 4;
        }
        BigInteger r = BigInteger.ONE.shiftLeft(RSA_WORDS * 32);
        int[] rr = toWords(r.multiply(r).mod(publicKey.getModulus()));
        for (int word : rr) {
            writeIntLE(blob, offset, word);
            offset += 4;
        }
        writeIntLE(blob, offset, publicKey.getPublicExponent().intValue());
        return blob;
    }

    private static int[] toWords(BigInteger value) {
        int[] words = new int[RSA_WORDS];
        byte[] bytes = value.toByteArray();
        for (int i = 0; i < RSA_WORDS; ++i) {
            int word = 0;
            for (int j = 0; j < 4; ++j) {
                int index = bytes.length - 1 - (i * 4 + j);
                if (index >= 0) {
                    word |= (bytes[index] & 0xff) << (j * 8);
                }
            }
            words[i] = word;
        }
        return words;
    }

    private static int montgomeryInverse(int firstWord) {
        long modulus = firstWord & 0xffffffffL;
        long inverse = 1;
        for (int i = 0; i < 5; ++i) {
            inverse = (inverse * (2 - modulus * inverse)) & 0xffffffffL;
        }
        return (int) (-inverse);
    }

    private static void writeIntLE(byte[] data, int offset, int value) {
        data[offset] = (byte) value;
        data[offset + 1] = (byte) (value >> 8);
        data[offset + 2] = (byte) (value >> 16);
        data[offset + 3] = (byte) (value >> 24);
    }
}
