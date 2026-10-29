package com.leosatdata.sdrmodem.model;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;

public class CommSettings {
	
	private SdrChannelConfig sdrSettings;
	private ModemSettings modemSettings;

	public CommSettings() {
		// do nothing
	}

	public CommSettings(DataInputStream dis) throws IOException {
		sdrSettings = new SdrChannelConfig(dis);
	}
	
	public SdrChannelConfig getSdrSettings() {
		return sdrSettings;
	}
	
	public void setSdrSettings(SdrChannelConfig sdrSettings) {
		this.sdrSettings = sdrSettings;
	}
	
	public ModemSettings getModemSettings() {
		return modemSettings;
	}
	
	public void setModemSettings(ModemSettings modemSettings) {
		this.modemSettings = modemSettings;
	}
	
	public void encode(DataOutputStream dos) throws IOException {
		sdrSettings.encode(dos);
		
	}

}
