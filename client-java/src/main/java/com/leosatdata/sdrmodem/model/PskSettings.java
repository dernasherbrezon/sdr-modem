package com.leosatdata.sdrmodem.model;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;

public class PskSettings {

	private long sampleRate;
	private int baudRate;
	private int bandwidth;
	private float rrcBeta;
	private int rrcDelay;
	private float costasBandwidth;
	private int symsyncFilterBankSize;

	public PskSettings() {
		// do nothing
	}

	public PskSettings(DataInputStream dis) throws IOException {
		// FIXME
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

	public int getBandwidth() {
		return bandwidth;
	}

	public void setBandwidth(int bandwidth) {
		this.bandwidth = bandwidth;
	}

	public float getRrcBeta() {
		return rrcBeta;
	}

	public void setRrcBeta(float rrcBeta) {
		this.rrcBeta = rrcBeta;
	}

	public int getRrcDelay() {
		return rrcDelay;
	}

	public void setRrcDelay(int rrcDelay) {
		this.rrcDelay = rrcDelay;
	}

	public float getCostasBandwidth() {
		return costasBandwidth;
	}

	public void setCostasBandwidth(float costasBandwidth) {
		this.costasBandwidth = costasBandwidth;
	}

	public int getSymsyncFilterBankSize() {
		return symsyncFilterBankSize;
	}

	public void setSymsyncFilterBankSize(int symsyncFilterBankSize) {
		this.symsyncFilterBankSize = symsyncFilterBankSize;
	}

	public void encode(DataOutputStream dos) throws IOException {
		// FIXME
//		o.writeLong(p.sampleRate());
//		o.writeInt(p.baudRate());
//		o.writeInt(p.bandwidth());
//		o.writeFloat(p.rrcBeta());
//		o.writeByte(p.rrcDelay());
//		o.writeFloat(p.costasBandwidth());
//		o.writeByte(p.symsyncFilterBankSize());
	}
}
