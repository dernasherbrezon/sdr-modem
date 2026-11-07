package com.leosatdata.sdrmodem.model;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;

public class MessageHeader {
	public static final int PROTOCOL_VERSION = 0;

	private int protocolVersion;
	private MessageType type;
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

	public MessageType getType() {
		return type;
	}

	public void setType(MessageType type) {
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
		protocolVersion = dis.readByte();
		type = MessageType.valueOfCode(dis.readByte());
		requestId = dis.readInt();
		messageLength = dis.readInt();
	}

	public void encode(DataOutputStream dos) throws IOException {
		dos.writeByte(protocolVersion);
		dos.writeByte(type.code());
		dos.writeInt(requestId);
		dos.writeInt(messageLength);
	}
}
