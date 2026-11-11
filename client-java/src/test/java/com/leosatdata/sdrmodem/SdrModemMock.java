package com.leosatdata.sdrmodem;

import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.EOFException;
import java.io.IOException;
import java.net.ServerSocket;
import java.net.Socket;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.leosatdata.sdrmodem.model.MessageHeader;
import com.leosatdata.sdrmodem.model.MessageType;
import com.leosatdata.sdrmodem.model.Response;
import com.leosatdata.sdrmodem.model.SoftBits;

public class SdrModemMock {

	private static final Logger LOG = LoggerFactory.getLogger(SdrModemMock.class);

	private Thread thread;
	private Thread clientThread;
	private ServerSocket server;
	private Socket client;
	private final int port;

	public SdrModemMock(int port) {
		this.port = port;
	}

	public void start() throws IOException {
		server = new ServerSocket(port);
		thread = new Thread(new Runnable() {

			@Override
			public void run() {
				try {
					while (!Thread.currentThread().isInterrupted()) {
						client = server.accept();
						synchronized (SdrModemMock.this) {
							if (!Thread.currentThread().isInterrupted()) {
								clientThread = new Thread(new Runnable() {

									@Override
									public void run() {
										MessageHeader header = new MessageHeader();
										MessageHeader responseHeader = new MessageHeader();
										Response response = new Response();
										DataInputStream dis = null;
										DataOutputStream dos = null;
										try {
											dis = new DataInputStream(client.getInputStream());
											dos = new DataOutputStream(client.getOutputStream());
										} catch (IOException e) {
											LOG.error("unable to setup streams", e);
											return;
										}

										while (!Thread.currentThread().isInterrupted()) {
											try {
												header.decode(dis);
												dis.skipBytes(header.getMessageLength());
											} catch (EOFException e) {
												return;
											} catch (IOException e) {
												LOG.error("invalid header", e);
												break;
											}
											response.setStatus(Response.STATUS_SUCCESS);
											byte[] payload;
											try {
												payload = encode(response);
											} catch (IOException e) {
												LOG.error("unable to encode response", e);
												break;
											}

											responseHeader.setProtocolVersion(MessageHeader.PROTOCOL_VERSION);
											responseHeader.setRequestId(header.getRequestId());
											responseHeader.setType(MessageType.RESPONSE);
											responseHeader.setMessageLength(payload.length);
											try {
												responseHeader.encode(dos);
												dos.write(payload);
												dos.flush();
											} catch (IOException e) {
												LOG.error("unable to send payload back", e);
												break;
											}

											if (header.getType() == MessageType.TX_FRAME) {
												responseHeader.setType(MessageType.TX_COMPLETE);
												responseHeader.setMessageLength(0);
												try {
													responseHeader.encode(dos);
													dos.flush();
												} catch (IOException e) {
													LOG.error("unable to send tx complete", e);
													break;
												}
											}
										}
									}
								}, "sdr-modem-client-handler");
								clientThread.setDaemon(true);
								clientThread.start();
							}
						}
					}
				} catch (IOException e) {
					return;
				}
			}
		}, "sdr-modem-mock");
		thread.setDaemon(true);
		thread.start();
	}

	public void stop() {
		if (thread != null) {
			thread.interrupt();
		}
		synchronized (this) {
			if (clientThread != null) {
				clientThread.interrupt();
			}
		}
		if (server != null) {
			try {
				server.close();
			} catch (IOException e) {
				// do nothing
			}
		}
		if (thread != null) {
			try {
				thread.join();
			} catch (InterruptedException e) {
				Thread.currentThread().interrupt();
			}
		}
	}

	public void sendData(byte[] data) {
		DataOutputStream dos = null;
		try {
			dos = new DataOutputStream(client.getOutputStream());
		} catch (IOException e) {
			LOG.error("unable to setup streams", e);
			return;
		}
		SoftBits bits = new SoftBits();
		bits.setFrequencyOffset(0);
		bits.setDataLength(data.length);
		bits.setData(data);
		try {
			byte[] payload = encodeSoftBits(bits);
			MessageHeader responseHeader = new MessageHeader();
			responseHeader.setProtocolVersion(MessageHeader.PROTOCOL_VERSION);
			responseHeader.setRequestId(0);
			responseHeader.setType(MessageType.SOFT_BITS);
			responseHeader.setMessageLength(payload.length);
			responseHeader.encode(dos);
			dos.write(payload);
			dos.flush();
		} catch (IOException e) {
			LOG.error("unable to send soft bits", e);
		}
	}

	private static byte[] encodeSoftBits(SoftBits data) throws IOException {
		ByteArrayOutputStream baos = new ByteArrayOutputStream();
		DataOutputStream dos = new DataOutputStream(baos);
		data.encode(dos);
		dos.close();
		return baos.toByteArray();
	}

	private static byte[] encode(Response response) throws IOException {
		ByteArrayOutputStream baos = new ByteArrayOutputStream();
		DataOutputStream dos = new DataOutputStream(baos);
		response.encode(dos);
		dos.close();
		return baos.toByteArray();
	}
}
