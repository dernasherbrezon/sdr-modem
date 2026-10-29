package com.leosatdata.sdrmodem.model;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;

public class MessageHeader {
	public static final int PROTOCOL_VERSION = 0;

	private int protocolVersion;
	private int type;
	private int requestId;
	private int messageLength;

	public MessageHeader() {
		// do nothing
	}

	public MessageHeader(DataInputStream dis) throws IOException {
		decode(dis);
	}

	public int getProtocolVersion() {
		return protocolVersion;
	}

	public void setProtocolVersion(int protocolVersion) {
		this.protocolVersion = protocolVersion;
	}

	public int getType() {
		return type;
	}

	public void setType(int type) {
		this.type = type;
	}

	public int getRequestId() {
		return requestId;
	}

	public void setRequestId(int requestId) {
		this.requestId = requestId;
	}

	public int getMessageLength() {
		return messageLength;
	}

	public void setMessageLength(int messageLength) {
		this.messageLength = messageLength;
	}

	public void decode(DataInputStream dis) throws IOException {
		// FIXME
	}

	public void encode(DataOutputStream dos) throws IOException {
		// FIXME
	}
}
