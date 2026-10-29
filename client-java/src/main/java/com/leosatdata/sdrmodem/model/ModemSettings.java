package com.leosatdata.sdrmodem.model;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;

public class ModemSettings {

	private GfskSettings gfsk;
	private PskSettings psk;
	private PskPmSettings pskpm;

	public ModemSettings() {
		// do nothing
	}

	public ModemSettings(DataInputStream dis) throws IOException {
		ModemType type = ModemType.valueOfCode(dis.readUnsignedByte());
		switch (type) {
		case GFSK: {
			gfsk = new GfskSettings(dis);
			break;
		}
		case BPSK:
		case DPSK:
		case SDPSK: {
			psk = new PskSettings(dis);
			break;
		}
		case PSK_PM: {
			pskpm = new PskPmSettings(dis);
			break;
		}
		default:
			throw new IllegalArgumentException("Unexpected value: " + type);
		}
	}

	public GfskSettings getGfsk() {
		return gfsk;
	}

	public void setGfsk(GfskSettings gfsk) {
		this.gfsk = gfsk;
	}

	public PskSettings getPsk() {
		return psk;
	}

	public void setPsk(PskSettings psk) {
		this.psk = psk;
	}

	public PskPmSettings getPskpm() {
		return pskpm;
	}

	public void setPskpm(PskPmSettings pskpm) {
		this.pskpm = pskpm;
	}
	
	public void encode(DataOutputStream dos) throws IOException {
		//FIXME
	}

}
