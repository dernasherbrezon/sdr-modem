package com.leosatdata.sdrmodem.model;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;

public class SdrChannelConfig {

	private long frequency;
	private long sampleRate;
	private float gain;

	public SdrChannelConfig() {
		// do nothing
	}

	public SdrChannelConfig(DataInputStream dis) throws IOException {
		frequency = dis.readLong();
		sampleRate = dis.readLong();
		gain = dis.readFloat();
	}

	public long getFrequency() {
		return frequency;
	}

	public void setFrequency(long frequency) {
		this.frequency = frequency;
	}

	public long getSampleRate() {
		return sampleRate;
	}

	public void setSampleRate(long sampleRate) {
		this.sampleRate = sampleRate;
	}

	public float getGain() {
		return gain;
	}

	public void setGain(float gain) {
		this.gain = gain;
	}

	public void encode(DataOutputStream dos) throws IOException {
		dos.writeLong(frequency);
		dos.writeLong(sampleRate);
		dos.writeFloat(gain);
	}
}
