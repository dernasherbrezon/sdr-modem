package com.leosatdata.sdrmodem.model;

public enum ModemType {
	GFSK(1), BPSK(2), DPSK(3), SDPSK(4), PSK_PM(5), UNKNOWN(255);

	private final int code;

	ModemType(int code) {
		this.code = code;
	}

	public int code() {
		return code;
	}

	public static ModemType valueOfCode(int code) {
		for (ModemType cur : values()) {
			if (cur.code == code) {
				return cur;
			}
		}
		return UNKNOWN;
	}
}
