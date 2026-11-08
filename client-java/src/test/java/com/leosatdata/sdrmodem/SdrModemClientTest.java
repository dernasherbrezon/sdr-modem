package com.leosatdata.sdrmodem;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import com.leosatdata.sdrmodem.model.CommSettings;
import com.leosatdata.sdrmodem.model.GfskSettings;
import com.leosatdata.sdrmodem.model.ModemSettings;
import com.leosatdata.sdrmodem.model.ModemType;
import com.leosatdata.sdrmodem.model.Response;
import com.leosatdata.sdrmodem.model.SdrChannelConfig;

public class SdrModemClientTest {

	private SdrModemMock mock;
	private SdrModemClient client;
	private SdrModemCallbackMock callback;

	@Test
	public void testCommands() throws Exception {
		assertSuccess(client.ping());
		assertSuccess(client.setRxParameters(createComm()));
		assertSuccess(client.setTxParameters(createComm()));
		assertSuccess(client.rxStart());
		assertSuccess(client.rxStop());
		client.shutdown();

		client.sendFrame(new byte[1]);
		assertTrue(callback.waitForTxComplete());

		byte[] data = new byte[] { 0x01 };
		mock.sendData(data);
		byte[] actual = callback.waitForRx();
		assertNotNull(actual);
		assertArrayEquals(data, actual);
	}

	@Before
	public void start() throws Exception {
		mock = new SdrModemMock(8001);
		mock.start();
		callback = new SdrModemCallbackMock();
		client = new SdrModemClient("localhost", 8001, 10000, callback);
		client.start();
	}

	@After
	public void stop() throws Exception {
		if (client != null) {
			client.close();
		}
		if (mock != null) {
			mock.stop();
		}
	}

	private static CommSettings createComm() {
		GfskSettings gfsk = new GfskSettings();
		gfsk.setBt(0.5f);
		gfsk.setSampleRate(240_000);
		ModemSettings modem = new ModemSettings();
		modem.setGfsk(gfsk);
		modem.setType(ModemType.GFSK);

		SdrChannelConfig sdr = new SdrChannelConfig();
		sdr.setFrequency(131_000_000);

		CommSettings result = new CommSettings();
		result.setModemSettings(modem);
		result.setSdrSettings(sdr);
		return result;
	}

	private static void assertSuccess(Response response) {
		assertNotNull(response);
		assertEquals(Response.STATUS_SUCCESS, response.getStatus());
	}
}
