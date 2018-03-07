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
import android.util.Log;
import com.google.vr.ndk.base.DaydreamApi;

public class GVRTransitionHelper {
	static public final int EXITVR_REQ_CODE = 77779;

	//exitFromVr with the game activity and OnActivityResult will take care of the result
	static public void transitionTo2D(final Activity vrActivity) {
		vrActivity.runOnUiThread(new Runnable() {
			@Override
			public void run() {
				try {
					DaydreamApi daydreamApi = (DaydreamApi)vrActivity.getClass().getMethod("getDaydreamApi").invoke(vrActivity);
					daydreamApi.exitFromVr(vrActivity, GVRTransitionHelper.EXITVR_REQ_CODE, null);
				} catch (Exception e) {
					Log.e("GVRTransitionHelper", "exception: " + e.toString());
				}
			}
		});
	}

	// show the backToVR button and let user colse the 2D activity and resume to the game
	static public void transitionToVR() {
		GVRTransition2DActivity.getActivity().showReturnButton();
	}

	// let native modules know that 2D transition is completed
	public static native void onTransitionTo2D(Activity activity);
}