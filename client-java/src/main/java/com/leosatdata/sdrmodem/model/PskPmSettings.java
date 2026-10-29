package com.leosatdata.sdrmodem.model;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;

public class PskPmSettings {

	private long sampleRate;
	private int subcarrierFrequency;
	private float carrierPllBandwidth;
	private int baudRate;
	private int subcarrierBandwidth;
	private float rrcBeta;
	private int rrcDelay;
	private float costasBandwidth;
	private int symsyncFilterBankSize;
	private float modulationIndex;

	public PskPmSettings() {
		// do nothing
	}

	public PskPmSettings(DataInputStream dis) throws IOException {
		// FIXME
	}

	public long getSampleRate() {
		return sampleRate;
	}

	public void setSampleRate(long sampleRate) {
		this.sampleRate = sampleRate;
	}

	public int getSubcarrierFrequency() {
		return subcarrierFrequency;
	}

	public void setSubcarrierFrequency(int subcarrierFrequency) {
		this.subcarrierFrequency = subcarrierFrequency;
	}

	public float getCarrierPllBandwidth() {
		return carrierPllBandwidth;
	}

	public void setCarrierPllBandwidth(float carrierPllBandwidth) {
		this.carrierPllBandwidth = carrierPllBandwidth;
	}

	public int getBaudRate() {
		return baudRate;
	}

	public void setBaudRate(int baudRate) {
		this.baudRate = baudRate;
	}

	public int getSubcarrierBandwidth() {
		return subcarrierBandwidth;
	}

	public void setSubcarrierBandwidth(int subcarrierBandwidth) {
		this.subcarrierBandwidth = subcarrierBandwidth;
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

	public float getModulationIndex() {
		return modulationIndex;
	}

	public void setModulationIndex(float modulationIndex) {
		this.modulationIndex = modulationIndex;
	}

	public void encode(DataOutputStream dos) throws IOException {
		// FIXME
//		o.writeLong(p.sampleRate());
//		o.writeInt(p.subcarrierFrequency());
//		o.writeFloat(p.carrierPllBandwidth());
//		o.writeInt(p.baudRate());
//		o.writeInt(p.subcarrierBandwidth());
//		o.writeFloat(p.rrcBeta());
//		o.writeByte(p.rrcDelay());
//		o.writeFloat(p.costasBandwidth());
//		o.writeByte(p.symsyncFilterBankSize());
//		o.writeFloat(p.modulationIndex());
	}
}
