package com.leosatdata.sdrmodem;

import java.io.BufferedInputStream;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.Closeable;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.EOFException;
import java.io.IOException;
import java.net.Socket;
import java.util.Map;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

import com.leosatdata.sdrmodem.model.CommSettings;
import com.leosatdata.sdrmodem.model.MessageHeader;
import com.leosatdata.sdrmodem.model.MessageType;
import com.leosatdata.sdrmodem.model.Response;

public class SdrModemClient implements Closeable, Runnable {

	private static final int MAX_MESSAGE_LENGTH = 64 * 1024;

	private final Socket socket;
	private final DataInputStream in;
	private final DataOutputStream out;
	private final SdrModemCallback callback;
	private final Map<Integer, CompletableFuture<Response>> pending = new ConcurrentHashMap<>();
	private int nextRequestId = 0;
	private Thread reader;
	private final long timeoutMillis;
	private volatile boolean closed;

	public SdrModemClient(String host, int port, long timeoutMillis, SdrModemCallback callback) throws IOException {
		this.socket = new Socket(host, port);
		this.in = new DataInputStream(new BufferedInputStream(socket.getInputStream()));
		this.out = new DataOutputStream(socket.getOutputStream());
		this.timeoutMillis = timeoutMillis;
		this.callback = callback;

	}

	public void start() {
		this.reader = new Thread(this, "sdr-modem-client");
		this.reader.setDaemon(true);
		this.reader.start();
	}

	public Response setRxParameters(CommSettings settings) throws IOException {
		return sendSync(MessageType.RX_COMM_PARAMETERS, encode(settings));
	}

	public Response setTxParameters(CommSettings settings) throws IOException {
		return sendSync(MessageType.TX_COMM_PARAMETERS, encode(settings));
	}

	public Response rxStart() throws IOException {
		return sendSync(MessageType.RX_START, new byte[0]);
	}

	public Response rxStop() throws IOException {
		return sendSync(MessageType.RX_STOP, new byte[0]);
	}

	public Response ping() throws IOException {
		return sendSync(MessageType.PING, new byte[0]);
	}

	public void sendFrame(byte[] frame) throws IOException {
		sendAsync(MessageType.TX_FRAME, frame);
	}

	/** Asks the server to disconnect. SHUTDOWN has no response. */
	public void shutdown() throws IOException {
		sendAsync(MessageType.SHUTDOWN, new byte[0]);
	}

	@Override
	public void close() throws IOException {
		if (closed) {
			return;
		}
		try {
			shutdown();
		} catch (IOException ignored) {
			// connection is already gone
		}
		closed = true;
		socket.close();
	}

	private Response sendSync(MessageType type, byte[] payload) throws IOException {
		int justSent = 0;
		try {
			CompletableFuture<Response> future = new CompletableFuture<>();
			sendAsync(type, payload);
			justSent = nextRequestId;
			pending.put(justSent, future);
			return future.get(timeoutMillis, TimeUnit.MILLISECONDS);
		} catch (InterruptedException e) {
			Thread.currentThread().interrupt();
			throw new IOException("interrupted while waiting for response", e);
		} catch (ExecutionException e) {
			throw new IOException("request failed", e.getCause());
		} catch (TimeoutException e) {
			throw new IOException("no response for " + type + " within " + timeoutMillis + "ms", e);
		} finally {
			pending.remove(justSent);
		}
	}

	private void sendAsync(MessageType type, byte[] payload) throws IOException {
		nextRequestId += 1;
		out.writeByte(MessageHeader.PROTOCOL_VERSION);
		out.writeByte(type.code());
		out.writeInt(nextRequestId);
		out.writeInt(payload.length);
		out.write(payload);
		out.flush();
	}

	@Override
	public void run() {
		try {
			// reduce GC while reading messages from socket
			MessageHeader header = new MessageHeader();
			while (!closed) {
				header.decode(in);
				if (header.getMessageLength() < 0 || header.getMessageLength() > MAX_MESSAGE_LENGTH) {
					throw new IOException("invalid message length: " + header.getMessageLength());
				}
				// always fully read payload in case protocol changes it should drain remaining bytes
				byte[] payload = new byte[header.getMessageLength()];
				in.readFully(payload);
				MessageType type = MessageType.valueOfCode(header.getType());
				if (type == MessageType.RESPONSE && payload.length >= Response.SIZE) {
					CompletableFuture<Response> future = pending.get(header.getRequestId());
					if (future != null) {
						Response response = new Response(new DataInputStream(new ByteArrayInputStream(payload)));
						future.complete(response);
					}
				} else if (type == MessageType.SOFT_BITS) {
					callback.onRx(payload);
				} else if (type == MessageType.TX_COMPLETE) {
					callback.onTx();
				}
			}
		} catch (EOFException e) {
			failPending(new IOException("server closed the connection"));
		} catch (IOException e) {
			failPending(closed ? new IOException("client closed") : e);
		}
	}

	private void failPending(IOException cause) {
		pending.values().forEach(f -> f.completeExceptionally(cause));
	}

	static byte[] encode(CommSettings settings) throws IOException {
		ByteArrayOutputStream buffer = new ByteArrayOutputStream();
		DataOutputStream dos = new DataOutputStream(buffer);
		settings.encode(dos);
		dos.flush();
		dos.close();
		return buffer.toByteArray();
	}

}
