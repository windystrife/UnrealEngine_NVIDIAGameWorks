/* Copyright 2016 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.google.vr.sdk.samples.transition;
import android.app.Activity;
import android.os.Bundle;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.widget.Button;
import android.content.Intent;
import com.google.vr.ndk.base.DaydreamApi;
import java.lang.Runnable;


public class GVRTransition2DActivity extends Activity
{
	private static final int EXIT_REQUEST_CODE = 777;
	public static GVRTransition2DActivity activity = null;
	public static GVRTransition2DActivity getActivity() {
		return GVRTransition2DActivity.activity;
	}

	protected void onCreate(Bundle savedInstanceState) {
		this.requestWindowFeature(Window.FEATURE_NO_TITLE);
		this.getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN, WindowManager.LayoutParams.FLAG_FULLSCREEN);
		super.onCreate(savedInstanceState);
		setContentView(R.layout.dialog);
		Button returnButton = (Button)findViewById(R.id.return_button);
		returnButton.setVisibility(View.GONE);
		returnButton.invalidate();
		GVRTransition2DActivity.activity = this;
	}

	protected void onStart() {
		super.onStart();
		GVRTransitionHelper.onTransitionTo2D(this);
	}

	public void showReturnButton() {
		runOnUiThread(new Runnable() {
		    public void run() {
						Button returnButton = (Button)GVRTransition2DActivity.activity.findViewById(R.id.return_button);
						returnButton.setVisibility(View.VISIBLE);
						returnButton.invalidate();
		    }
		});
	}

	public void returnToVR(View view) {
		GVRTransition2DActivity.activity = null;
		finish();
	}
}