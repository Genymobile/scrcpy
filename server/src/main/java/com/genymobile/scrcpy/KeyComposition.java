package com.genymobile.scrcpy;

import java.util.HashMap;
import java.util.Map;

/**
 * Decompose accented characters.
 * <p>
 * For example, {@link #decompose(char) decompose('é')} returns {@code "\u0301e"}.
 * <p>
 * This is useful for injecting key events to generate the expected character ({@link android.view.KeyCharacterMap#getEvents(char[])}
 * KeyCharacterMap.getEvents()} returns {@code null} with input {@code "é"} but works with input {@code "\u0301e"}).
 * <p>
 * See <a href="https://source.android.com/devices/input/key-character-map-files#behaviors">diacritical dead key characters</a>.
 */
public final class KeyComposition {

    private static final String KEY_DEAD_GRAVE = "\u0300";
    private static final String KEY_DEAD_ACUTE = "\u0301";
    private static final String KEY_DEAD_CIRCUMFLEX = "\u0302";
    private static final String KEY_DEAD_TILDE = "\u0303";
    private static final String KEY_DEAD_UMLAUT = "\u0308";

    private static final Map<Character, String> COMPOSITION_MAP = createDecompositionMap();

    private KeyComposition() {
        // not instantiable
    }

    public static String decompose(char c) {
        return COMPOSITION_MAP.get(c);
    }

    private static String grave(char c) {
        return KEY_DEAD_GRAVE + c;
    }

    private static String acute(char c) {
        return KEY_DEAD_ACUTE + c;
    }

    private static String circumflex(char c) {
        return KEY_DEAD_CIRCUMFLEX + c;
    }

    private static String tilde(char c) {
        return KEY_DEAD_TILDE + c;
    }

    private static String umlaut(char c) {
        return KEY_DEAD_UMLAUT + c;
    }

    private static Map<Character, String> createDecompositionMap() {
        Map<Character, String> map = new HashMap<>();
        map.put('À', grave('A'));
        map.put('È', grave('E'));
        map.put('Ì', grave('I'));
        map.put('Ò', grave('O'));
        map.put('Ù', grave('U'));
        map.put('à', grave('a'));
        map.put('è', grave('e'));
        map.put('ì', grave('i'));
        map.put('ò', grave('o'));
        map.put('ù', grave('u'));
        map.put('Ǹ', grave('N'));
        map.put('ǹ', grave('n'));
        map.put('Ẁ', grave('W'));
        map.put('ẁ', grave('w'));
        map.put('Ỳ', grave('Y'));
        map.put('ỳ', grave('y'));

        map.put('Á', acute('A'));
        map.put('É', acute('E'));
        map.put('Í', acute('I'));
        map.put('Ó', acute('O'));
        map.put('Ú', acute('U'));
        map.put('Ý', acute('Y'));
        map.put('á', acute('a'));
        map.put('é', acute('e'));
        map.put('í', acute('i'));
        map.put('ó', acute('o'));
        map.put('ú', acute('u'));
        map.put('ý', acute('y'));
        map.put('Ć', acute('C'));
        map.put('ć', acute('c'));
        map.put('Ĺ', acute('L'));
        map.put('ĺ', acute('l'));
        map.put('Ń', acute('N'));
        map.put('ń', acute('n'));
        map.put('Ŕ', acute('R'));
        map.put('ŕ', acute('r'));
        map.put('Ś', acute('S'));
        map.put('ś', acute('s'));
        map.put('Ź', acute('Z'));
        map.put('ź', acute('z'));
        map.put('Ǵ', acute('G'));
        map.put('ǵ', acute('g'));
        map.put('Ḉ', acute('Ç'));
        map.put('ḉ', acute('ç'));
        map.put('Ḱ', acute('K'));
        map.put('ḱ', acute('k'));
        map.put('Ḿ', acute('M'));
        map.put('ḿ', acute('m'));
        map.put('Ṕ', acute('P'));
        map.put('ṕ', acute('p'));
        map.put('Ẃ', acute('W'));
        map.put('ẃ', acute('w'));

        map.put('Â', circumflex('A'));
        map.put('Ê', circumflex('E'));
        map.put('Î', circumflex('I'));
        map.put('Ô', circumflex('O'));
        map.put('Û', circumflex('U'));
        map.put('â', circumflex('a'));
        map.put('ê', circumflex('e'));
        map.put('î', circumflex('i'));
        map.put('ô', circumflex('o'));
        map.put('û', circumflex('u'));
        map.put('Ĉ', circumflex('C'));
        map.put('ĉ', circumflex('c'));
        map.put('Ĝ', circumflex('G'));
        map.put('ĝ', circumflex('g'));
        map.put('Ĥ', circumflex('H'));
        map.put('ĥ', circumflex('h'));
        map.put('Ĵ', circumflex('J'));
        map.put('ĵ', circumflex('j'));
        map.put('Ŝ', circumflex('S'));
        map.put('ŝ', circumflex('s'));
        map.put('Ŵ', circumflex('W'));
        map.put('ŵ', circumflex('w'));
        map.put('Ŷ', circumflex('Y'));
        map.put('ŷ', circumflex('y'));
        map.put('Ẑ', circumflex('Z'));
        map.put('ẑ', circumflex('z'));

        map.put('Ã', tilde('A'));
        map.put('Ñ', tilde('N'));
        map.put('Õ', tilde('O'));
        map.put('ã', tilde('a'));
        map.put('ñ', tilde('n'));
        map.put('õ', tilde('o'));
        map.put('Ĩ', tilde('I'));
        map.put('ĩ', tilde('i'));
        map.put('Ũ', tilde('U'));
        map.put('ũ', tilde('u'));
        map.put('Ẽ', tilde('E'));
        map.put('ẽ', tilde('e'));
        map.put('Ỹ', tilde('Y'));
        map.put('ỹ', tilde('y'));

        map.put('Ä', umlaut('A'));
        map.put('Ë', umlaut('E'));
        map.put('Ï', umlaut('I'));
        map.put('Ö', umlaut('O'));
        map.put('Ü', umlaut('U'));
        map.put('ä', umlaut('a'));
        map.put('ë', umlaut('e'));
        map.put('ï', umlaut('i'));
        map.put('ö', umlaut('o'));
        map.put('ü', umlaut('u'));
        map.put('ÿ', umlaut('y'));
        map.put('Ÿ', umlaut('Y'));
        map.put('Ḧ', umlaut('H'));
        map.put('ḧ', umlaut('h'));
        map.put('Ẅ', umlaut('W'));
        map.put('ẅ', umlaut('w'));
        map.put('Ẍ', umlaut('X'));
        map.put('ẍ', umlaut('x'));
        map.put('ẗ', umlaut('t'));

        return map;
    }
}
