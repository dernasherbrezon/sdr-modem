package com.leosatdata.sdrmodem;

public class SdrModemCallbackMock implements SdrModemCallback {

	private final static long DEFAULT_TIMEOUT = 10000;
	private final Object lock = new Object();
	private byte[] rx;
	private boolean txCalled = false;

	@Override
	public void onRx(byte[] data) {
		synchronized (lock) {
			rx = new byte[data.length];
			System.arraycopy(data, 0, rx, 0, data.length);
			lock.notifyAll();
		}
	}

	@Override
	public void onTx() {
		synchronized (lock) {
			txCalled = true;
			lock.notifyAll();
		}
	}

	@Override
	public void onCarrierAcquired() {
		// do nothing
	}

	@Override
	public void onCarrierLost() {
		// do nothing
	}

	public boolean waitForTxComplete() {
		synchronized (lock) {
			long start = System.currentTimeMillis();
			while (!txCalled) {
				try {
					lock.wait(DEFAULT_TIMEOUT);
					if (System.currentTimeMillis() - start >= DEFAULT_TIMEOUT) {
						break;
					}
				} catch (InterruptedException e) {
					Thread.currentThread().interrupt();
					break;
				}
			}
			return txCalled;
		}
	}

	public byte[] waitForRx() {
		synchronized (lock) {
			long start = System.currentTimeMillis();
			while (rx == null) {
				try {
					lock.wait(DEFAULT_TIMEOUT);
					if (System.currentTimeMillis() - start >= DEFAULT_TIMEOUT) {
						break;
					}
				} catch (InterruptedException e) {
					Thread.currentThread().interrupt();
					break;
				}
			}
			byte[] result = rx;
			rx = null;
			return result;
		}
	}

}
