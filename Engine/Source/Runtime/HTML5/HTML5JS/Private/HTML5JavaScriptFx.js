// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

var UE_JavascriptLibrary = {
  UE_SendAndRecievePayLoad: function (url, indata, insize, outdataptr, outsizeptr) {

    // NOTE: C++ calling this function are written with syncronus logic
    // otherwise, use: UE_MakeHTTPDataRequest() -- see below

    var _url = Pointer_stringify(url);

    var request = new XMLHttpRequest();
    request.UE_fetch = {
        url : _url
      , outsizeptr : outsizeptr
      , outdataptr : outdataptr
      , timeout : 2 // allow 2 retries
    }

    request.ontimeout = function(e) {
      if ( ! this.UE_fetch.timeout ) {
        console.log("Fetching " + this.UE_fetch.url + " timed out");
        Module.HEAP32[this.UE_fetch.outsizeptr >> 2] = 0;
        Module.HEAP32[this.UE_fetch.outdataptr >> 2] = 0;
        return;
      }
      this.UE_fetch.timeout--;
      return UE_JSlib.UE_SendAndRecievePayLoad( this );
    }

    if (insize && indata) {
      var postData = Module.HEAP8.subarray(indata, indata + insize);
      request.UE_fetch.postData = postData;
    }
    return UE_JSlib.UE_SendAndRecievePayLoad( request );
  },

  // ================================================================================
  // ================================================================================

  UE_SaveGame: function (name, indata, insize) {
    var _name = Pointer_stringify(name);
    var gamedata = Module.HEAPU8.subarray(indata, indata + insize);
    // local storage only takes strings, we need to convert string to base64 before storing.
    var b64encoded = base64EncArr(gamedata);
    $.jStorage.set(_name, b64encoded);
    return true;
  },

  UE_LoadGame: function (name, outdataptr, outsizeptr) {
    var _name = Pointer_stringify(name);
    // local storage only takes strings, we need to convert string to base64 before storing.
    var b64encoded = $.jStorage.get(_name);
    if (b64encoded === null)
      return false;
    var decodedArray = base64DecToArr(b64encoded);
    // copy back the decoded array.
    var outdata = Module._malloc(decodedArray.length);
    // view the allocated data as a HEAP8.
    var dest = Module.HEAPU8.subarray(outdata, outdata + decodedArray.length);
    // copy back.
    for (var i = 0; i < decodedArray.length; ++i) {
      dest[i] = decodedArray[i];
    }
    Module.HEAP32[outsizeptr >> 2] = decodedArray.length;
    Module.HEAP32[outdataptr >> 2] = outdata;
    return true;
  },

  UE_DeleteSavedGame: function (name){
    var _name = Pointer_stringify(name);
    return $.jStorage.deleteKey(_name);
  },

  UE_DoesSaveGameExist: function (name){
    var _name = Pointer_stringify(name);
    var keys = $.jStorage.index();
    for (var i in keys)
    {
    	if (keys[i] == _name)
    		return true;
    }
    return false;
  },

  // ================================================================================
  // ================================================================================

  UE_MessageBox: function (type, message, caption ) {
    // type maps to EAppMsgType::Type
    var text = Pointer_stringify(message);
    if (!type) return confirm(text);
    alert(text);
    return 1;
  },

  // ================================================================================
  // ================================================================================

  UE_GetCurrentCultureName: function (address, outsize) {
    var culture_name = navigator.language || navigator.browserLanguage;
    if (culture_name.lenght >= outsize)
      return 0;
    Module.writeAsciiToMemory(culture_name, address);
    return 1;
  },

  // ================================================================================
  // ================================================================================

  UE_MakeHTTPDataRequest: function (ctx, url, verb, payload, payloadsize, headers, async, freeBuffer, onload, onerror, onprogress) {
    var _url = Pointer_stringify(url);
    var _verb = Pointer_stringify(verb);
    var _headers = Pointer_stringify(headers);

    var xhr = new XMLHttpRequest();
    xhr.UE_fetch = {
        verb : _verb
      , url : _url
      , async : !!async
      , postData: null
      , timeout : 2 // allow 2 retries
    }
    if (_verb === "POST") {
      xhr.UE_fetch.postData = Module.HEAP8.subarray(payload, payload + payloadsize);
    }

    xhr.open(_verb, _url, !!async);
    xhr.responseType = 'arraybuffer';

    // set all headers.
    var _headerArray = _headers.split("%");
    for(var headerArrayidx = 0; headerArrayidx < _headerArray.length; headerArrayidx++){
      var header = _headerArray[headerArrayidx].split(":");
      // NOTE: as of Safari 9.0 -- no leading whitespace is allowed on setRequestHeader's 2nd parameter: "value"
      xhr.setRequestHeader(header[0], header[1].trim());
    }

    // Onload event handler
    xhr.addEventListener('load', function (e) {
      if (xhr.status === 200 || _url.substr(0, 4).toLowerCase() !== "http") {
        // headers
        var headers = xhr.getAllResponseHeaders();
        var header_byteArray = new TextEncoder('utf-8').encode(headers);
        var header_buffer = _malloc(header_byteArray.length);
        HEAPU8.set(header_byteArray, header_buffer);

        // response
        var byteArray = new Uint8Array(xhr.response);
        var buffer = _malloc(byteArray.length);
        HEAPU8.set(byteArray, buffer);

        if (onload)
          Runtime.dynCall('viiii', onload, [ctx, buffer, byteArray.length, header_buffer]);
        if (freeBuffer) // seems POST reqeusts keeps the buffer
          _free(buffer);
        _free(header_buffer);
      } else {
        if (onerror)
          Runtime.dynCall('viii', onerror, [ctx, xhr.status, xhr.statusText]);
      }
    });

    // Onerror event handler
    xhr.addEventListener('error', function (e) {
      if ( xhr.responseURL == "" )
        console.log('ERROR: Cross-Origin Resource Sharing [CORS] check FAILED'); // common error that's not quite so clear during onerror callbacks
      if (onerror)
        Runtime.dynCall('viii', onerror, [ctx, xhr.status, xhr.statusText]);
    });

    // Onprogress event handler
    xhr.addEventListener('progress', function (e) {
      if (onprogress)
        Runtime.dynCall('viii', onprogress, [ctx, e.loaded, e.lengthComputable || e.lengthComputable === undefined ? e.total : 0]);
    });

    // Ontimeout event handler
    xhr.addEventListener('timeout', function (e) {
      if ( ! this.UE_fetch.timeout ) {
      console.log("Fetching " + this.UE_fetch.url + " timed out");
        if (onerror)
          Runtime.dynCall('viii', onerror, [ctx, xhr.status, xhr.statusText]);
        return;
      }
      this.UE_fetch.timeout--;
      xhr.open(this.UE_fetch.verb, this.UE_fetch.url, this.UE_fetch.async);
      xhr.responseType = 'arraybuffer';
      xhr.send(xhr.UE_fetch.postData);
    });

    // Bypass possible browser redirection limit
    try {
      if (xhr.channel instanceof Ci.nsIHttpChannel)
        xhr.channel.redirectionLimit = 0;
    } catch (ex) { }

    xhr.send(xhr.UE_fetch.postData);
  },

  // ================================================================================
  // ================================================================================

  // persistant OBJECT accessible within JS code
  $UE_JSlib: {

    // --------------------------------------------------------------------------------
    // UE_SendAndRecievePayLoad -- FOR REAL
    // --------------------------------------------------------------------------------

    UE_SendAndRecievePayLoad: function (request) {
  
      if ( request.UE_fetch.postData ) {
        request.open('POST', request.UE_fetch.url, false);
        request.overrideMimeType('text\/plain; charset=x-user-defined');
        request.send(request.UE_fetch.postData);
      } else {
        request.open('GET', request.UE_fetch.url, false);
        request.send();
      }
  
      if (request.status != 200) {
        console.log("Fetching " + _url + " failed: " + request.responseText);
  
        Module.HEAP32[request.UE_fetch.outsizeptr >> 2] = 0;
        Module.HEAP32[request.UE_fetch.outdataptr >> 2] = 0;
  
        return;
      }
  
      // we got the XHR result as a string.  We need to write this to Module.HEAP8[outdataptr]
      var replyString = request.responseText;
      var replyLength = replyString.length;
  
      var outdata = Module._malloc(replyLength);
      if (!outdata) {
        console.log("Failed to allocate " + replyLength + " bytes in heap for reply");
  
        Module.HEAP32[request.UE_fetch.outsizeptr >> 2] = 0;
        Module.HEAP32[request.UE_fetch.outdataptr >> 2] = 0;
  
        return;
      }
  
      // Copy from the result-string into the heap.
      var replyDest = Module.HEAP8.subarray(outdata, outdata + replyLength);
      for (var i = 0; i < replyLength; ++i) {
        replyDest[i] = replyString.charCodeAt(i) &  0xff;
      }
  
      Module.HEAP32[request.UE_fetch.outsizeptr >> 2] = replyLength;
      Module.HEAP32[request.UE_fetch.outdataptr >> 2] = outdata;
    },


    // --------------------------------------------------------------------------------
    // onBeforeUnload
    // --------------------------------------------------------------------------------

    onBeforeUnload_callbacks:[], // ARRAY of {callback:c++function, ctx:[c++objects]}
    // ........................................
    onBeforeUnload_debug_helper: function(dummyfile) {
      // this function will basically fetch a dummy file to see if onbeforeunload
      // events are working properly -- look for the http request in your web logs
      var debug_xhr = new XMLHttpRequest();
      debug_xhr.open("GET", dummyfile, false);
      // ----------------------------------------
      debug_xhr.addEventListener('load', function (e) {
        if (debug_xhr.status === 200 || _url.substr(0, 4).toLowerCase() !== "http")
          console.log("debug_xhr.response: " + debug_xhr.response);
        else
          console.log("debug_xhr.response: FAILED");
      });
      // ----------------------------------------
      debug_xhr.addEventListener('error', function (e) {
        console.log("debug_xhr.onerror: FAILED");
      });
      // ----------------------------------------
      debug_xhr.send(null);
    },
    // ........................................
    onBeforeUnload: function (e) {
 //   UE_JSlib.onBeforeUnload_debug_helper("START_of_onBeforeUnload_test.js"); // disable for shipping

      // to trap window closing or back button pressed, use 'msg' code
//    var msg = 'trap window closing -- i.e. by accident';
//    (e||window.event).returnValue = msg; // Gecko + IE

      window.removeEventListener("beforeunload", UE_JSlib.onBeforeUnload, false);
      var callbacks = UE_JSlib.onBeforeUnload_callbacks;
      UE_JSlib.onBeforeUnload_callbacks=[]; // make unregister calls do nothing
      for ( var x in callbacks ) {
        var contexts = callbacks[0].ctx;
        for ( var y in contexts ) {
          try { // jic
            Runtime.dynCall('vi', callbacks[x].callback, [contexts[y]]);
          } catch (e) {}
        }
      }

//    UE_JSlib.onBeforeUnload_debug_helper("END_of_onBeforeUnload_test.js"); // disable for shipping

//    return msg; // Gecko + Webkit, Safari, Chrome etc.
    },
    // ........................................
    onBeforeUnload_setup: function() {
      window.addEventListener("beforeunload", UE_JSlib.onBeforeUnload);
    },
  },

  // ================================================================================
  // ================================================================================

  // --------------------------------------------------------------------------------
  // onBeforeUnload
  // --------------------------------------------------------------------------------

  UE_Reset_OnBeforeUnload: function () {
    UE_JSlib.onBeforeUnload_callbacks=[];
  },
  // ........................................
  UE_Register_OnBeforeUnload: function (ctx, callback) {
    // check for existing
    for ( var x in UE_JSlib.onBeforeUnload_callbacks ) {
      if ( UE_JSlib.onBeforeUnload_callbacks[x].callback != callback )
        continue;
      var contexts = UE_JSlib.onBeforeUnload_callbacks[x].ctx;
      for ( var y in contexts ) {
        if ( contexts[y] == ctx )
          return; // already registered
      }
      UE_JSlib.onBeforeUnload_callbacks[x].ctx[contexts.length] = ctx; // new ctx
      return;
    }
    // add new callback
    UE_JSlib.onBeforeUnload_callbacks[UE_JSlib.onBeforeUnload_callbacks.length] = { callback:callback, ctx:[ctx] };

    // fire up the onbeforeunload listener
    UE_JSlib.onBeforeUnload_setup();
    UE_JSlib.onBeforeUnload_setup = function() {}; // noop
  },
  // ........................................
  UE_UnRegister_OnBeforeUnload: function (ctx, callback) {
    for ( var x in UE_JSlib.onBeforeUnload_callbacks ) {
      if ( UE_JSlib.onBeforeUnload_callbacks[x].callback != callback )
        continue;
      var contexts = UE_JSlib.onBeforeUnload_callbacks[x].ctx;
      for ( var y in contexts ) {
        if ( contexts[i] != ctx )
          continue;
        UE_JSlib.onBeforeUnload_callbacks[x].ctx.splice(y,1);
        if (!UE_JSlib.onBeforeUnload_callbacks[x].ctx.length)
          UE_JSlib.onBeforeUnload_callbacks.splice(x,1);
        return;
      }
    }
  },

  // --------------------------------------------------------------------------------
  // GSystemResolution - helpers to obtain game's resolution for JS
  // --------------------------------------------------------------------------------

  UE_GSystemResolution: function( resX, resY ) {
    UE_JSlib.UE_GSystemResolution_ResX = function() {
      return Runtime.dynCall('i', resX, []);
    };
    UE_JSlib.UE_GSystemResolution_ResY = function() {
      return Runtime.dynCall('i', resY, []);
    };
  },
 
  UE_EngineRegisterCanvasResizeListener: function(listener) {
    UE_JSlib.UE_CanvasSizeChanged = function() {
      Runtime.dynCall('v', listener);
    }
  },

  UE_BrowserWebGLVersion: function() {
    return Module['WEBGL_VERSION'];
  }
};

autoAddDeps(UE_JavascriptLibrary,'$UE_JSlib');
mergeInto(LibraryManager.library, UE_JavascriptLibrary);
