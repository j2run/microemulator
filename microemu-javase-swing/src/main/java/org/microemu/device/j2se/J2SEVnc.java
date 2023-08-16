package org.microemu.device.j2se;

import java.awt.image.BufferedImage;

import org.microemu.device.MutableImage;
import org.microemu.device.j2se.J2SEDeviceDisplay;
import java.util.ArrayDeque;
import java.util.Queue;

// yuh ---
public class J2SEVnc {
	private native boolean drawPixels(byte[] pixcels, int width, int height, int loopEvent);
	private native boolean updateProcess();
	private native boolean updateProcessV2(int loopEvent);
	private native void setObject();
	private int fps = -1;
	private int fpsFrameTime = -1;
	private long fpsTime = 0;
	private boolean hasConnection = false;

    private byte[] pixelBytes = new byte[0];
	private int width;
	private int height;
	private boolean hasUpdated = false;

	private Thread thread;
	private J2SEMutableImage displayImage = null;
	private J2SEDeviceDisplay deviceDisplay = null;

    public static J2SEVnc instance = new J2SEVnc();

    J2SEVnc() {
        setObject();
    }

	public void setDevice(J2SEDeviceDisplay j2seDeviceDisplay) {
		deviceDisplay = j2seDeviceDisplay;
	}

	public void createThreadChill() {
		if (thread == null) {
			System.out.println("\n\nCreate new gp thread\n\n");
			thread = new Thread() {
				@Override
				public void run() {
					while (true) {
						if (hasConnection) {
							byte[] pixels = null;
							if (hasUpdated) {
								// System.out.println("render");
								synchronized (pixelBytes) {
									hasConnection = drawPixels(pixelBytes, width, height, 1);
									hasUpdated = false;
								}
							} else {
								// System.out.println("render - no");
								if (java.lang.System.currentTimeMillis() - fpsTime > fpsFrameTime * 2) {
									// System.out.println("render - force");
									deviceDisplay.repaint(0, 0, width, height);
									hasConnection = updateProcessV2(10);
								} else {
									hasConnection = updateProcessV2(1);
								}
							}
							
							try {
								Thread.sleep(1000 / 15);
							} catch (InterruptedException e) {
								e.printStackTrace();
							}
						} else {
							// System.out.println("not connect");
							hasConnection = updateProcess();
							try {
								Thread.sleep(100);
							} catch (InterruptedException e) {
								e.printStackTrace();
							}
						}
					}
				}
			};
			thread.start();
		}
	}

    private void setFps(int fps) {
		this.fps = fps;
		this.fpsFrameTime = 1000 / fps;
		System.out.println("accept!" + fps + " " + this.fpsFrameTime);
	};

	private boolean canNextFrame() {
		if (fps < 1) {
			return true;
		}
		if (java.lang.System.currentTimeMillis() - fpsTime > fpsFrameTime) {
			fpsTime = java.lang.System.currentTimeMillis();
			return true;
		}
		return false;
	}

    public void draw(MutableImage displayImage) {
		if (displayImage != null && (hasConnection && canNextFrame())) {
			int[] pixels = displayImage.getData();
			if (pixelBytes.length != pixels.length * 4) {
				pixelBytes = new byte[pixels.length * 4];
				width = displayImage.getWidth();
				height = displayImage.getHeight();
			}
			
			synchronized (pixelBytes) {
				int pixel;
				for (int i = 0; i < pixels.length; i++) {
					pixel = pixels[i];
					pixelBytes[i * 4 + 0] = (byte)((((pixel >> 16) & 0xFF) >> 4) << 4);
					pixelBytes[i * 4 + 1] = (byte)((((pixel >> 8) & 0xFF) >> 4) << 4);
					pixelBytes[i * 4 + 2] = (byte)((( pixel & 0xFF) >> 5) << 5);
					pixelBytes[i * 4 + 3] = (byte)((((pixel >> 24) & 0xFF) >> 5) << 5);
				}
				// System.out.println("push frame");
				hasUpdated = true;
			}
		}
    }

	public boolean isRender() {
		return hasConnection && canNextFrame();
	}
}