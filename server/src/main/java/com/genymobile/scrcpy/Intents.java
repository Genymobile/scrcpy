package com.genymobile.scrcpy;

import java.io.IOException;
import java.util.*;

enum Intents {
	START(1),
	STOP(30),
	CLEANED(31),
	;

	public static final String SCRCPY_PREFIX = "com.genymobile.scrcpy.";

	int shift;
	Intents(int shift) {
		this.shift = shift;
	}

	public static EnumSet<Intents> fromBitSet(BitSet bits) {
		EnumSet<Intents> es = EnumSet.allOf(Intents.class);

		Iterator<Intents> it = es.iterator();
		Intents intent;
		while (it.hasNext()) {
			intent = it.next();
			if (!bits.get(intent.shift - 1)) {
				it.remove();
			}
		}
		return es;
	}

	public static String scrcpyPrefix(String unprefixed){
		return SCRCPY_PREFIX + unprefixed;
	}
}
