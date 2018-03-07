// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
//This file needs to be here so the "ant" build step doesnt fail when looking for a /src folder.

package com.epicgames.ue4;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentSender.SendIntentException;
import android.content.ServiceConnection;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.RemoteException;
import com.android.vending.billing.util.Base64;
import com.android.vending.billing.IInAppBillingService;
import com.android.vending.billing.util.Purchase;
import java.util.ArrayList;
import org.json.JSONException;
import org.json.JSONObject;

public class GooglePlayStoreHelper implements StoreHelper
{
    // Billing response codes
	// taken from http://developer.android.com/google/play/billing/billing_reference.html Table 1
    public static final int BILLING_RESPONSE_RESULT_OK = 0;
    public static final int BILLING_RESPONSE_RESULT_USER_CANCELED = 1;
	public static final int BILLING_RESPONSE_RESULT_SERVICE_UNAVAILABLE = 2;
    public static final int BILLING_RESPONSE_RESULT_BILLING_UNAVAILABLE = 3;
    public static final int BILLING_RESPONSE_RESULT_ITEM_UNAVAILABLE = 4;
    public static final int BILLING_RESPONSE_RESULT_DEVELOPER_ERROR = 5;
    public static final int BILLING_RESPONSE_RESULT_ERROR = 6;
    public static final int BILLING_RESPONSE_RESULT_ITEM_ALREADY_OWNED = 7;
    public static final int BILLING_RESPONSE_RESULT_ITEM_NOT_OWNED = 8;

	// Keys for the responses from InAppBillingService
	// taken from http://developer.android.com/google/play/billing/billing_reference.html Table 3
	public static final String RESPONSE_CODE = "RESPONSE_CODE";
	public static final String RESPONSE_INAPP_PURCHASE_DATA = "INAPP_PURCHASE_DATA";
	public static final String RESPONSE_INAPP_SIGNATURE = "INAPP_DATA_SIGNATURE";

	// taken from https://developer.android.com/google/play/billing/billing_integrate.html#purchase
	public static final String RESPONSE_GET_SKU_DETAILS_LIST = "DETAILS_LIST";
	public static final String RESPONSE_BUY_INTENT = "BUY_INTENT";
	public static final String RESPONSE_INAPP_ITEM_LIST = "INAPP_PURCHASE_ITEM_LIST";
	public static final String RESPONSE_INAPP_PURCHASE_DATA_LIST = "INAPP_PURCHASE_DATA_LIST";
	public static final String RESPONSE_INAPP_SIGNATURE_LIST = "INAPP_DATA_SIGNATURE_LIST";
	public static final String INAPP_CONTINUATION_TOKEN = "INAPP_CONTINUATION_TOKEN";

	// Item types
	public static final String ITEM_TYPE_INAPP = "inapp";

	// some fields on the getSkuDetails response bundle
	public static final String GET_SKU_DETAILS_ITEM_LIST = "ITEM_ID_LIST";

	// Our IAB helper interface provided by google.
	private IInAppBillingService mService;

	// Flag that determines whether the store is ready to use.
	private boolean bIsIapSetup;

	// Output device for log messages.
	private Logger Log;

	// Cache access to the games activity.
	private GameActivity gameActivity;

	// The google play license key.
	private String productKey;

	private final int UndefinedFailureResponse = -1;

	public interface PurchaseLaunchCallback
	{
		void launchForResult(PendingIntent pendingIntent, int requestCode);
	}

	public GooglePlayStoreHelper(String InProductKey, GameActivity InGameActivity, final Logger InLog)
	{
		// IAP is not ready to use until the service is instantiated.
		bIsIapSetup = false;

		Log = InLog;
		Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::GooglePlayStoreHelper");

		gameActivity = InGameActivity;
		productKey = InProductKey;
		
		Intent serviceIntent = new Intent("com.android.vending.billing.InAppBillingService.BIND");
		serviceIntent.setPackage("com.android.vending");
		gameActivity.bindService(serviceIntent, mServiceConn, Context.BIND_AUTO_CREATE);
	}

	
	///////////////////////////////////////////////////////
	// The StoreHelper interfaces implementation for Google Play Store.

	/**
	 * Determine whether the store is ready for purchases.
	 */
	public boolean IsAllowedToMakePurchases()
	{
		Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::IsAllowedToMakePurchases");
		return bIsIapSetup;
	}

	/**
	 * Query product details for the provided skus
	 */
	public boolean QueryInAppPurchases(String[] InProductIDs)
	{
		Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::QueryInAppPurchases");
		ArrayList<String> skuList = new ArrayList<String> ();
		
		for (String productId : InProductIDs)
		{
			Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::QueryInAppPurchases - Querying " + productId);
			skuList.add(productId);
		}

		Bundle querySkus = new Bundle();
		querySkus.putStringArrayList(GET_SKU_DETAILS_ITEM_LIST, skuList);

		try
		{
			Bundle skuDetails = mService.getSkuDetails(3, gameActivity.getPackageName(), ITEM_TYPE_INAPP, querySkus);

			int response = skuDetails.getInt(RESPONSE_CODE);
			Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::QueryInAppPurchases - Response " + response + " Bundle:" + skuDetails.toString());
			if (response == BILLING_RESPONSE_RESULT_OK)
			{
				ArrayList<String> productIds = new ArrayList<String>();
				ArrayList<String> titles = new ArrayList<String>();
				ArrayList<String> descriptions = new ArrayList<String>();
				ArrayList<String> prices = new ArrayList<String>();
				ArrayList<Float> pricesRaw = new ArrayList<Float>();
				ArrayList<String> currencyCodes = new ArrayList<String>();

				ArrayList<String> responseList = skuDetails.getStringArrayList(RESPONSE_GET_SKU_DETAILS_LIST);
				for (String thisResponse : responseList)
				{
					JSONObject object = new JSONObject(thisResponse);
				
					String productId = object.getString("productId");
					productIds.add(productId);
					Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::QueryInAppPurchases - Parsing details for: " + productId);
				
					String title = object.getString("title");
					titles.add(title);
					Log.debug("[GooglePlayStoreHelper] - title: " + title);

					String description = object.getString("description");
					descriptions.add(description);
					Log.debug("[GooglePlayStoreHelper] - description: " + description);

					String price = object.getString("price");
					prices.add(price);
					Log.debug("[GooglePlayStoreHelper] - price: " + price);

					double priceRaw = object.getDouble("price_amount_micros") / 1000000.0;
					pricesRaw.add((float)priceRaw);
					Log.debug("[GooglePlayStoreHelper] - price_amount_micros: " + priceRaw);

					String currencyCode = object.getString("price_currency_code");
					currencyCodes.add(currencyCode);
					Log.debug("[GooglePlayStoreHelper] - price_currency_code: " + currencyCode);
				}

				float[] pricesRawPrimitive = new float[pricesRaw.size()];
				for (int i = 0; i < pricesRaw.size(); i++)
				{
					pricesRawPrimitive[i] = pricesRaw.get(i);
				}

				Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::QueryInAppPurchases " + productIds.size() + " items - Success!");
				nativeQueryComplete(response, productIds.toArray(new String[productIds.size()]), titles.toArray(new String[titles.size()]), descriptions.toArray(new String[descriptions.size()]), prices.toArray(new String[prices.size()]), pricesRawPrimitive, currencyCodes.toArray(new String[currencyCodes.size()]));
				Log.debug("[GooglePlayStoreHelper] - nativeQueryComplete done!");
			}
			else
			{
				Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::QueryInAppPurchases - Failed!");
				nativeQueryComplete(response, null, null, null, null, null, null);
			}
		}
		catch(Exception e)
		{
			Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::QueryInAppPurchases - Failed! " + e.getMessage());
			nativeQueryComplete(UndefinedFailureResponse, null, null, null, null, null, null);
		}

		return true;
	}
	
	/**
	 * Start the purchase flow for a particular sku
	 */
	final int purchaseIntentIdentifier = 1001;
	public boolean BeginPurchase(String ProductID)
	{
		try
		{
			String devPayload = GenerateDevPayload(ProductID);
			Bundle buyIntentBundle = null;
			int response = -1;

			if (gameActivity.IsInVRMode())
			{
				Bundle bundle = new Bundle();
				bundle.putBoolean("vr", true);
				response = mService.isBillingSupportedExtraParams(7, gameActivity.getPackageName(), ITEM_TYPE_INAPP, bundle);
				if (response == BILLING_RESPONSE_RESULT_OK)
				{
					Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::BeginPurchase - v7 VR purchase" + ProductID);
					buyIntentBundle = mService.getBuyIntentExtraParams(7, gameActivity.getPackageName(), ProductID, ITEM_TYPE_INAPP, devPayload, bundle);
				} 
				else
				{
					Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::BeginPurchase - v3 IAB purchase:" + ProductID);
					buyIntentBundle = mService.getBuyIntent(3, gameActivity.getPackageName(), ProductID, ITEM_TYPE_INAPP, devPayload);
				}
			}
			else
			{
				Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::BeginPurchase - v3 IAB purchase:" + ProductID);
				buyIntentBundle = mService.getBuyIntent(3, gameActivity.getPackageName(), ProductID, ITEM_TYPE_INAPP, devPayload);
			}

			response = buyIntentBundle.getInt(RESPONSE_CODE);
			if (response == BILLING_RESPONSE_RESULT_OK)
			{
				Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::BeginPurchase - Starting Intent to buy " + ProductID);
				PendingIntent pendingIntent = buyIntentBundle.getParcelable(RESPONSE_BUY_INTENT);
				if (pendingIntent != null)
				{
					PurchaseLaunchCallback callback =  gameActivity.getPurchaseLaunchCallback();
					if (callback != null)
					{
						callback.launchForResult(pendingIntent, purchaseIntentIdentifier);
					}
					else
					{
						gameActivity.startIntentSenderForResult(pendingIntent.getIntentSender(), purchaseIntentIdentifier, new Intent(), Integer.valueOf(0), Integer.valueOf(0), Integer.valueOf(0));
					}
				}
				else
				{
					Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::BeginPurchase - pendingIntent was null, possible non consumed item");
					nativePurchaseComplete(UndefinedFailureResponse, ProductID, "", "", "");
				}
			}
			else
			{
				Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::BeginPurchase - Failed with error: " + TranslateServerResponseCode(response));
				nativePurchaseComplete(response, ProductID, "", "", "");
			}
		}
		catch(Exception e)
		{
			Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::BeginPurchase - Failed! " + e.getMessage());
			nativePurchaseComplete(UndefinedFailureResponse, ProductID, "", "", "");
		}

		return true;
	}

	/**
	 * Restore previous purchases the user may have made.
	 */
	public boolean RestorePurchases(String[] InProductIDs, boolean[] bConsumable)
	{
		Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::RestorePurchases");
		ArrayList<String> ownedSkus = new ArrayList<String>();
		ArrayList<String> purchaseDataList = new ArrayList<String>();
		ArrayList<String> signatureList = new ArrayList<String>();
		
		// On first pass the continuation token should be null. 
		// This will allow us to gather large sets of purchased items recursively
		int responseCode = GatherOwnedPurchaseData(ownedSkus, purchaseDataList, signatureList, null);
		if (responseCode == BILLING_RESPONSE_RESULT_OK)
		{
			Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::RestorePurchases - User has previously purchased " + ownedSkus.size() + " inapp products" );

			final ArrayList<String> f_ownedSkus = ownedSkus;
			final ArrayList<String> f_purchaseDataList = purchaseDataList;
			final ArrayList<String> f_signatureList = signatureList;
			final String[] RestoreProductIDs = InProductIDs;
			final boolean[] bRestoreConsumableFlags = bConsumable;

			Handler mainHandler = new Handler(Looper.getMainLooper());
			mainHandler.post(new Runnable()
			{
				@Override
				public void run()
				{
					try
					{
						Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::RestorePurchases - Now consuming any purchases that may have been missed.");
						ArrayList<String> productTokens = new ArrayList<String>();
						ArrayList<String> receipts = new ArrayList<String>();
						int cachedResponse = 0;

						// @todo possible bug, restores ALL ownedSkus, not just those provided by caller in RestoreProductIDs
						for (int Idx = 0; Idx < f_ownedSkus.size(); Idx++)
						{
							String purchaseData = f_purchaseDataList.get(Idx);
							String dataSignature = f_signatureList.get(Idx);

							try
							{
								Purchase purchase = new Purchase(ITEM_TYPE_INAPP, purchaseData, dataSignature);
								productTokens.add(purchase.getToken());

								boolean bTryToConsume = false;
								int consumeResponse = 0;
							
								// This is assuming that all purchases should be consumed. Consuming a purchase that is meant to be a one-time purchase makes it so the
								// user is able to buy it again. Also, it makes it so the purchase will not be able to be restored again in the future.

								for (int Idy = 0; Idy < RestoreProductIDs.length; Idy++)
								{
									if( purchase.getSku().equals(RestoreProductIDs[Idy]) )
									{
										if(Idy < bRestoreConsumableFlags.length)
										{
											bTryToConsume = bRestoreConsumableFlags[Idy];
											Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::RestorePurchases - Found Consumable Flag for Product " + purchase.getSku() + " bConsumable = " + bTryToConsume);
										}
										break;
									}
								}

								if (bTryToConsume)
								{
									Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::RestorePurchases - Attempting to consume " + purchase.getSku());
									consumeResponse = mService.consumePurchase(3, gameActivity.getPackageName(), purchase.getToken());
								}

								if (consumeResponse == 0)
								{
									Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::RestorePurchases - Purchase restored for " + purchase.getSku());
									String receipt = Base64.encode(purchase.getOriginalJson().getBytes());
									receipts.add(receipt);
								}
								else
								{
									Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::RestorePurchases - consumePurchase failed for " + purchase.getSku() + " with error " + consumeResponse);
									receipts.add("");
									cachedResponse = consumeResponse;
								}
							}
							catch (JSONException e)
							{
								Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::RestorePurchases - Failed to parse receipt! " + e.getMessage());
								productTokens.add("");
								receipts.add("");
							}
						}
						Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::RestorePurchases - Success!");
						nativeRestorePurchasesComplete(cachedResponse, f_ownedSkus.toArray(new String[f_ownedSkus.size()]), productTokens.toArray(new String[productTokens.size()]), receipts.toArray(new String[receipts.size()]), f_signatureList.toArray(new String[f_signatureList.size()]));
					}
					catch (Exception e)
					{
						Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::RestorePurchases - consumePurchase failed. " + e.getMessage());
						nativeRestorePurchasesComplete(UndefinedFailureResponse, null, null, null, null);
					}
				}
			});
		}
		else
		{
			nativeRestorePurchasesComplete(responseCode, null, null, null, null);
			Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::RestorePurchases - Failed to collect existing purchases");
			return false;
		}

		return true;
	}

	public boolean QueryExistingPurchases()
	{
		Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::QueryExistingPurchases");
		ArrayList<String> ownedSkus = new ArrayList<String>();
		ArrayList<String> purchaseDataList = new ArrayList<String>();
		ArrayList<String> signatureList = new ArrayList<String>();
		
		// On first pass the continuation token should be null. 
		// This will allow us to gather large sets of purchased items recursively
		int responseCode = GatherOwnedPurchaseData(ownedSkus, purchaseDataList, signatureList, null);
		if (responseCode == BILLING_RESPONSE_RESULT_OK)
		{
			Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::QueryExistingPurchases - User has previously purchased " + ownedSkus.size() + " inapp products" );

			ArrayList<String> productTokens = new ArrayList<String>();
			ArrayList<String> receipts = new ArrayList<String>();

			for (int Idx = 0; Idx < ownedSkus.size(); Idx++)
			{
				String purchaseData = purchaseDataList.get(Idx);
				String dataSignature = signatureList.get(Idx);

				try
				{
					Purchase purchase = new Purchase(ITEM_TYPE_INAPP, purchaseData, dataSignature);
					productTokens.add(purchase.getToken());

					String receipt = Base64.encode(purchase.getOriginalJson().getBytes());
					receipts.add(receipt);
				}			
				catch (JSONException e)
				{
					Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::QueryExistingPurchases - Failed to parse receipt! " + e.getMessage());
					productTokens.add("");
					receipts.add("");
				}
			}

			Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::QueryExistingPurchases - Success!");
			nativeQueryExistingPurchasesComplete(responseCode, ownedSkus.toArray(new String[ownedSkus.size()]), productTokens.toArray(new String[productTokens.size()]), receipts.toArray(new String[receipts.size()]), signatureList.toArray(new String[signatureList.size()]));
			return true;
		}
		else
		{
			Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::QueryExistingPurchases - Failed to collect existing purchases" +  TranslateServerResponseCode(responseCode));
			nativeQueryExistingPurchasesComplete(responseCode, null, null, null, null);
			return false;
		}
	}

	public void ConsumePurchase(String purchaseToken)
	{
		Log.debug("[GooglePlayStoreHelper] - Beginning ConsumePurchase: " + purchaseToken);

		final String f_purchaseToken = purchaseToken;

		Handler mainHandler = new Handler(Looper.getMainLooper());
		mainHandler.post(new Runnable()
		{
			@Override
			public void run()
			{
				try
				{
					Log.debug("[GooglePlayStoreHelper] - Consuming token: " + f_purchaseToken);
					int consumeResponse = mService.consumePurchase(3, gameActivity.getPackageName(), f_purchaseToken);
					if (consumeResponse == BILLING_RESPONSE_RESULT_OK)
					{
						Log.debug("[GooglePlayStoreHelper] - ConsumePurchase success");
					}
					else
					{
						Log.debug("[GooglePlayStoreHelper] - ConsumePurchase failed with error " + TranslateServerResponseCode(consumeResponse));
					}
				}
				catch(Exception e)
				{
					Log.debug("[GooglePlayStoreHelper] - ConsumePurchase failed. " + e.getMessage());
				}
			}
		});
	}

	///////////////////////////////////////////////////////
	// IInAppBillingService service registration and unregistration

	private ServiceConnection mServiceConn = new ServiceConnection()
	{
		@Override
		public void onServiceDisconnected(ComponentName name)
		{
			Log.debug("[GooglePlayStoreHelper] - ServiceConnection::onServiceDisconnected");
			mService = null;
			bIsIapSetup = false;
		}

		@Override
		public void onServiceConnected(ComponentName name, IBinder service)
		{
			Log.debug("[GooglePlayStoreHelper] - ServiceConnection::onServiceConnected");
			mService = IInAppBillingService.Stub.asInterface(service);
			bIsIapSetup = true;

			try 
			{
                Log.debug("Checking for in-app billing 3 support.");

                // check for in-app billing v3 support
                int response = mService.isBillingSupported(3, gameActivity.getPackageName(), ITEM_TYPE_INAPP);
                if (response != BILLING_RESPONSE_RESULT_OK) 
				{
					Log.debug("In-app billing version 3 NOT supported for " + gameActivity.getPackageName() + " error " + response);
                }
				else
				{
					Log.debug("In-app billing version 3 supported for " + gameActivity.getPackageName());
				}
			}
            catch (RemoteException e)
			{
				e.printStackTrace();
				return;
            }
		}
	};

	
	///////////////////////////////////////////////////////
	// Game Activity/Context driven methods we need to listen for.
	
	/**
	 * On Destory we should unbind our IInAppBillingService service
	 */
	public void onDestroy()
	{
		Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::onDestroy");

		if (mService != null)
		{
			gameActivity.unbindService(mServiceConn);
		}
	}
	
	/**
	 * Route taken by the Purchase workflow. We listen for our purchaseIntentIdentifier request code and
	 * handle the response accordingly
	 */
	public boolean onActivityResult(int requestCode, int resultCode, Intent data)
	{
		Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::onActivityResult");

		if (requestCode == purchaseIntentIdentifier)
		{
		    if (data == null) 
			{
				Log.debug("Null data in purchase activity result.");
				nativePurchaseComplete(UndefinedFailureResponse, "", "", "", "");
				return true;
			}

			int responseCode = data.getIntExtra(RESPONSE_CODE, 0);
			if (resultCode == Activity.RESULT_OK)
			{
				final String purchaseData = data.getStringExtra(RESPONSE_INAPP_PURCHASE_DATA);
				final String dataSignature = data.getStringExtra(RESPONSE_INAPP_SIGNATURE);

				Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::onActivityResult - Processing purchase result. Response Code: " + TranslateServerResponseCode(responseCode));
				if (responseCode == BILLING_RESPONSE_RESULT_OK)
				{
					Log.debug("Purchase data: " + purchaseData);
					Log.debug("Data signature: " + dataSignature);
					Log.debug("Extras: " + data.getExtras());

					if (purchaseData == null || dataSignature == null)
					{
						Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::onActivityResult - Either purchaseData or dataSignature is null");
						nativePurchaseComplete(UndefinedFailureResponse, "", "", "", "");
					}

					try
					{
						final Purchase purchase = new Purchase(ITEM_TYPE_INAPP, purchaseData, dataSignature);
						final String sku = purchase.getSku();
						String developerPayload = purchase.getDeveloperPayload();
						if (VerifyPayload(developerPayload, sku))
						{
							Handler mainHandler = new Handler(Looper.getMainLooper());
							mainHandler.post(new Runnable()
							{
								@Override
								public void run()
								{
									String receipt = Base64.encode(purchase.getOriginalJson().getBytes());
									nativePurchaseComplete(BILLING_RESPONSE_RESULT_OK, sku, purchase.getToken(), receipt, purchase.getSignature());
								}
							});
						}
						else
						{
							Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::onActivityResult - Failed for verify developer payload for " + sku);
							nativePurchaseComplete(UndefinedFailureResponse, sku, "", "", "");
						}
					}
					catch (JSONException e)
					{
						Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::onActivityResult - Failed for purchase request! " + e.getMessage());
						nativePurchaseComplete(UndefinedFailureResponse, "", "", "", "");
					}
				}
				else
				{
					Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::onActivityResult - Failed for purchase request!. " + TranslateServerResponseCode(responseCode));
					nativePurchaseComplete(responseCode, "", "", "", "");
				}
			}
			else if (resultCode == Activity.RESULT_CANCELED)
			{
				Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::onActivityResult - Purchase canceled." + TranslateServerResponseCode(responseCode));
				nativePurchaseComplete(BILLING_RESPONSE_RESULT_USER_CANCELED, "", "", "", "");
			}
			else
			{
				Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::onActivityResult - Purchase failed. Result code: " +
					Integer.toString(resultCode) + ". Response: " + TranslateServerResponseCode(responseCode));
				nativePurchaseComplete(UndefinedFailureResponse, "", "", "", "");
			}

			return true;
		}

		return false;
	}

	
	///////////////////////////////////////////////////////
	// Internal helper functions that deal assist with various IAB related events
	
	/**
	 * Create a UE4 specific unique string that will be used to verify purchases are legit.
	 */
	private String GenerateDevPayload( String ProductId )
	{
		return "ue4." + ProductId;
	}
	
	/**
	 * Check the returned payload matches one for the product we are buying.
	 */
	private boolean VerifyPayload( String ExistingPayload, String ProductId )
	{
		String GeneratedPayload = GenerateDevPayload(ProductId);
		Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::VerifyPayload, ExistingPayload: " + ExistingPayload);
		Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::VerifyPayload, GeneratedPayload: " + GeneratedPayload);

		return ExistingPayload.equals(GeneratedPayload);
	}
	
	/**
	 * Get a text tranlation of the Response Codes returned by google play.
	 */
	private String TranslateServerResponseCode(final int serverResponseCode)
	{
		// taken from http://developer.android.com/google/play/billing/billing_reference.html
		switch(serverResponseCode)
		{
			case BILLING_RESPONSE_RESULT_OK:
				return "Success";
			case BILLING_RESPONSE_RESULT_USER_CANCELED:
				return "User pressed back or canceled a dialog";
			case BILLING_RESPONSE_RESULT_SERVICE_UNAVAILABLE:
				return "Network connection is down";
			case BILLING_RESPONSE_RESULT_BILLING_UNAVAILABLE:
				return "Billing API version is not supported for the type requested";
			case BILLING_RESPONSE_RESULT_ITEM_UNAVAILABLE:
				return "Requested product is not available for purchase";
			case BILLING_RESPONSE_RESULT_DEVELOPER_ERROR:
				return "Invalid arguments provided to the API. This error can also indicate that the application was not correctly signed or properly set up for In-app Billing in Google Play, or does not have the necessary permissions in its manifest";
			case BILLING_RESPONSE_RESULT_ERROR:
				return "Fatal error during the API action";
			case BILLING_RESPONSE_RESULT_ITEM_ALREADY_OWNED:
				return "Failure to purchase since item is already owned";
			case BILLING_RESPONSE_RESULT_ITEM_NOT_OWNED:
				return "Failure to consume since item is not owned";
			default:
				return "Unknown Server Response Code";
		}
	}
	
	/**
	 * Recursive functionality to gather all of the purchases owned by a user.
	 * if the user owns a lot of products then we need to getPurchases again with a continuationToken
	 */
	private int GatherOwnedPurchaseData(ArrayList<String> inOwnedSkus, ArrayList<String> inPurchaseDataList, ArrayList<String> inSignatureList, String inContinuationToken)
	{
		int responseCode = UndefinedFailureResponse;
		try
		{
			Bundle ownedItems = mService.getPurchases(3, gameActivity.getPackageName(), ITEM_TYPE_INAPP, inContinuationToken);

			responseCode = ownedItems.getInt(RESPONSE_CODE);
			Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::GatherOwnedPurchaseData - getPurchases result. Response Code: " + TranslateServerResponseCode(responseCode));
			if (responseCode == BILLING_RESPONSE_RESULT_OK)
			{
				ArrayList<String> ownedSkus = ownedItems.getStringArrayList(RESPONSE_INAPP_ITEM_LIST);
				ArrayList<String> purchaseDataList = ownedItems.getStringArrayList(RESPONSE_INAPP_PURCHASE_DATA_LIST);
				ArrayList<String> signatureList = ownedItems.getStringArrayList(RESPONSE_INAPP_SIGNATURE_LIST);
				String continuationToken = ownedItems.getString(INAPP_CONTINUATION_TOKEN);

				for (int Idx = 0; Idx < purchaseDataList.size(); Idx++)
				{
					String sku = ownedSkus.get(Idx);
					inOwnedSkus.add(sku);

					String purchaseData = purchaseDataList.get(Idx);
					inPurchaseDataList.add(purchaseData);

					String signature = signatureList.get(Idx);
					inSignatureList.add(signature);
				}

				// if continuationToken != null, call getPurchases again
				// and pass in the token to retrieve more items
				if (continuationToken != null)
				{
					return GatherOwnedPurchaseData(inOwnedSkus, inPurchaseDataList, inSignatureList, inContinuationToken);
				}
			}
		}
		catch (Exception e)
		{
			Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::GatherOwnedPurchaseData - Failed for purchase request!. " + e.getMessage());
		}

		return responseCode;
	}

	// Callback that notify the C++ implementation that a task has completed
	public native void nativeQueryComplete(int responseCode, String[] productIDs, String[] titles, String[] descriptions, String[] prices, float[] pricesRaw, String[] currencyCodes );
	public native void nativePurchaseComplete(int responseCode, String ProductId, String ProductToken, String ReceiptData, String Signature);
	public native void nativeRestorePurchasesComplete(int responseCode, String[] ProductIds, String[] ProductTokens, String[] ReceiptsData, String[] Signatures);
	public native void nativeQueryExistingPurchasesComplete(int responseCode, String[] ProductIds, String[] ProductTokens, String[] ReceiptsData, String[] Signatures);
}

