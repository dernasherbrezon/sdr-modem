package com.leosatdata.sdrmodem.model;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;

public class Response {
	
	public static final int SIZE = 2;

	private int status;
	private int details;

	public static final int STATUS_SUCCESS = 0;
	public static final int STATUS_FAILURE = 1;

	public static final int DETAILS_NO_DETAILS = 0;
	public static final int DETAILS_INVALID_REQUEST = 1;
	public static final int DETAILS_INTERNAL_ERROR = 2;
	public static final int DETAILS_TX_IS_BEING_USED = 3;
	public static final int DETAILS_RX_IS_BEING_USED = 4;

	public Response() {
		// do nothing
	}

	public Response(DataInputStream dis) throws IOException {
		decode(dis);
	}

	public int getStatus() {
		return status;
	}

	public void setStatus(int status) {
		this.status = status;
	}

	public int getDetails() {
		return details;
	}

	public void setDetails(int details) {
		this.details = details;
	}

	public boolean isSuccess() {
		return status == STATUS_SUCCESS;
	}

	public void decode(DataInputStream dis) throws IOException {
		// FIXME
	}

	public void encode(DataOutputStream dos) throws IOException {
		// FIXME
	}
}
