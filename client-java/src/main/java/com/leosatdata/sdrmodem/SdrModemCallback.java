package com.leosatdata.sdrmodem;

import com.leosatdata.sdrmodem.model.SoftBits;

public interface SdrModemCallback {

	void onRx(SoftBits data);

	void onTx();
	
	void onCarrierAcquired();
	
	void onCarrierLost();

}
