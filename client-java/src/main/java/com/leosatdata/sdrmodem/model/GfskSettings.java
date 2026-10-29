package com.leosatdata.sdrmodem.model;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;

public class GfskSettings {

	private long sampleRate;
	private int baudRate;
	private long deviation;
	private int bandwidth;
	private float bt;
	private boolean useDcBlock;
	private long syncword;
	private int syncwordBits;

	public GfskSettings() {
		// do nothing
	}

	public GfskSettings(DataInputStream dis) throws IOException {
		//FIXME
	}

	public long getSampleRate() {
		return sampleRate;
	}

	public void setSampleRate(long sampleRate) {
		this.sampleRate = sampleRate;
	}

	public int getBaudRate() {
		return baudRate;
	}

	public void setBaudRate(int baudRate) {
		this.baudRate = baudRate;
	}

	public long getDeviation() {
		return deviation;
	}

	public void setDeviation(long deviation) {
		this.deviation = deviation;
	}

	public int getBandwidth() {
		return bandwidth;
	}

	public void setBandwidth(int bandwidth) {
		this.bandwidth = bandwidth;
	}

	public float getBt() {
		return bt;
	}

	public void setBt(float bt) {
		this.bt = bt;
	}

	public boolean isUseDcBlock() {
		return useDcBlock;
	}

	public void setUseDcBlock(boolean useDcBlock) {
		this.useDcBlock = useDcBlock;
	}

	public long getSyncword() {
		return syncword;
	}

	public void setSyncword(long syncword) {
		this.syncword = syncword;
	}

	public int getSyncwordBits() {
		return syncwordBits;
	}

	public void setSyncwordBits(int syncwordBits) {
		this.syncwordBits = syncwordBits;
	}

	public void encode(DataOutputStream dos) throws IOException {
		// FIXME
//		o.writeLong(g.sampleRate());
//		o.writeInt(g.baudRate());
//		o.writeLong(g.deviation());
//		o.writeInt(g.bandwidth());
//		o.writeFloat(g.bt());
//		o.writeByte(g.useDcBlock() ? 1 : 0);
//		o.writeLong(g.syncword());
//		o.writeInt(g.syncwordBits());
	}
}
