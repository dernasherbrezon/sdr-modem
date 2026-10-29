package com.leosatdata.sdrmodem;

public interface SdrModemCallback {

	void onRx(byte[] data);

	void onTx();

}
