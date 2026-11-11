package com.leosatdata.sdrmodem.model;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;

public class SoftBits {

	private int frequencyOffset;
	private byte[] data;
	private int dataLength;

	public SoftBits() {
		// do nothing
	}

	public SoftBits(DataInputStream dis) throws IOException {
		frequencyOffset = dis.readInt();
		dataLength = dis.readInt();
		data = new byte[(int) dataLength];
		dis.readFully(data);
	}

	public int getFrequencyOffset() {
		return frequencyOffset;
	}

	public void setFrequencyOffset(int frequencyOffset) {
		this.frequencyOffset = frequencyOffset;
	}

	public int getDataLength() {
		return dataLength;
	}

	public void setDataLength(int dataLength) {
		this.dataLength = dataLength;
	}

	public byte[] getData() {
		return data;
	}

	public void setData(byte[] data) {
		this.data = data;
	}

	public void encode(DataOutputStream dos) throws IOException {
		dos.writeInt(frequencyOffset);
		dos.writeInt(dataLength);
		dos.write(data);
	}

}
