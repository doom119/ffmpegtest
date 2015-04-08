package com.doom119.ffmpegtest;

import android.app.Activity;
import android.os.Bundle;
import android.view.View;

public class MainActivity extends Activity
{
	static
	{
		System.loadLibrary("FFmpegTest");
		System.loadLibrary("avcodec-56");
		System.loadLibrary("avformat-56");
		System.loadLibrary("avutil-54");
		System.loadLibrary("swresample-1");
		System.loadLibrary("swscale-3");
	}
	
	public static native void prepareFFmpeg(String path);

	@Override
	protected void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);
		setContentView(R.layout.activity_main);
	}
	
	public void onClick(View v)
	{
		String path = "/mnt/sdcard/20150325_000547_18088/20150325_000547_39933.vf";
		prepareFFmpeg(path);
	}
}
