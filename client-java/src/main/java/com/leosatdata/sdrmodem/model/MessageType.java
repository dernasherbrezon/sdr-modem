package com.leosatdata.sdrmodem.model;

public enum MessageType {
	
	RX_COMM_PARAMETERS(0), SHUTDOWN(1), PING(2), TX_COMM_PARAMETERS(3), RESPONSE(4), SOFT_BITS(5), TX_FRAME(6), RX_START(7), RX_STOP(8), TX_COMPLETE(9);

	private final int code;

	MessageType(int code) {
		this.code = code;
	}

	public int code() {
		return code;
	}

	public static MessageType valueOfCode(int code) {
		for (MessageType type : values()) {
			if (type.code == code) {
				return type;
			}
		}
		return null;
	}
}
