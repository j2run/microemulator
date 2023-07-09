package org.microemu.device.j2se;

import java.awt.image.BufferedImage;

import org.microemu.device.MutableImage;

// yuh ---
public class J2SEVnc {
	private native boolean drawPixels(byte[] pixcels, int width, int height, int loopEvent);
	private native boolean updateProcess();
	private native void setObject();
	private int fps = -1;
	private int fpsFrameTime = -1;
	private long fpsTime = 0;
	private boolean hasConnection = false;
    private byte[] pixelBytes = new byte[0];

    public static J2SEVnc instance = new J2SEVnc();

    J2SEVnc() {
        setObject();
    }

    private void setFps(int fps) {
		this.fps = fps;
		this.fpsFrameTime = 1000 / fps;
		System.out.println("accept!" + fps + " " + this.fpsFrameTime);
	};

	private boolean canUpNext() {
		if (fps < 1) {
			return true;
		}
		if (java.lang.System.currentTimeMillis() - fpsTime > fpsFrameTime) {
			fpsTime = java.lang.System.currentTimeMillis();
			return true;
		}
		return false;
	}

    public void draw(MutableImage displayImage, int loopEvent, boolean forceRender) {
        if (forceRender || (hasConnection && canUpNext())) {
			int[] pixels = displayImage.getData();
            if (pixels.length != pixels.length * 4) {
                pixelBytes = new byte[pixels.length * 4];
            }
			int pixel;
            int width = displayImage.getWidth();
            int height = displayImage.getHeight();
			for (int i = 0; i < pixels.length; i++) {
				pixel = pixels[i];
				// pixelBytes[i * 4 + 0] = (byte)((pixel >> 16) & 0xFF);
				// pixelBytes[i * 4 + 1] = (byte)((pixel >> 8) & 0xFF);
				// pixelBytes[i * 4 + 2] = (byte)( pixel & 0xFF);
				// pixelBytes[i * 4 + 3] = (byte)((pixel >> 24) & 0xFF);

				pixelBytes[i * 4 + 0] = (byte)((((pixel >> 16) & 0xFF) >> 4) << 4);
				pixelBytes[i * 4 + 1] = (byte)((((pixel >> 8) & 0xFF) >> 4) << 4);
				pixelBytes[i * 4 + 2] = (byte)((( pixel & 0xFF) >> 5) << 5);
				pixelBytes[i * 4 + 3] = (byte)((((pixel >> 24) & 0xFF) >> 5) << 5);
			}
			hasConnection = drawPixels(pixelBytes, width, height, loopEvent);
		} else {
			hasConnection = updateProcess();
		}
    }

	public boolean isRender() {
		return hasConnection && canUpNext();
	}
}