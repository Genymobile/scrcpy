package com.genymobile.scrcpy.ime;

final class ImeRecovery {
    private ImeRecovery() {
        // not instantiable
    }

    static String findFallbackIme(String imeList, String excludedIme) {
        for (String line : imeList.split("[\\r\\n]+")) {
            String imeId = line.trim();
            if (!imeId.isEmpty() && !excludedIme.equals(imeId)) {
                return imeId;
            }
        }
        return null;
    }
}
