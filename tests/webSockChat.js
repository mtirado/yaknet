  var secureCb;
  var secureCbLabel;
  var wsUri;
  var wsUsername;
  var consoleLog;
  var connectBut;
  var disconnectBut;
  var sendMessage;
  var sendBut;
  var clearLogBut;

  var ByteBuffer = dcodeIO.ByteBuffer;

  function echoHandlePageLoad()
  {
 
    wsUsername = document.getElementById("wsUsername");
    if (wsUsername.value === "") {
        wsUsername.value = "someone";
    }

    wsUri = document.getElementById("wsUri");
    if (wsUri.value === "") {
        wsUri.value = "ws://connect.level00.com:1337";
    }
    
    connectBut = document.getElementById("connect");
    connectBut.onclick = doConnect;
    
    disconnectBut = document.getElementById("disconnect");
    disconnectBut.onclick = doDisconnect;
    
    sendMessage = document.getElementById("sendMessage");

    document.getElementById('sendMessage').onkeypress = function(e){
    if (!e) e = window.event;
    var keyCode = e.keyCode || e.which;
    if (keyCode == '13'){
      doSend();
      return false;
    }
    }

    sendBut = document.getElementById("send");
    sendBut.onclick = doSend;

    consoleLog = document.getElementById("consoleLog");

    clearLogBut = document.getElementById("clearLogBut");
    clearLogBut.onclick = clearLog;

    setGuiConnected(false);

    document.getElementById("disconnect").onclick = doDisconnect;
    document.getElementById("send").onclick = doSend;

  }

  function doConnect()
  {
    if (window.MozWebSocket)
    {
        logToConsole('<span style="color: red;"><strong>Info:</strong> This browser supports WebSocket using the MozWebSocket constructor</span>');
        window.WebSocket = window.MozWebSocket;
    }
    else if (!window.WebSocket)
    {
        logToConsole('<span style="color: red;"><strong>Error:</strong> This browser does not have support for WebSocket</span>');
        return;
    }

    var uri = wsUri.value;
    websocket = new WebSocket(uri);
    //use arrayBuffer so byteBuffer can wrap it!
    websocket.binaryType = "arraybuffer";
    websocket.onopen = function(evt) { onOpen(evt) };
    websocket.onclose = function(evt) { onClose(evt) };
    websocket.onmessage = function(evt) { onMessage(evt) };
    websocket.onerror = function(evt) { onError(evt) };

  }
  
  function doDisconnect()
  {
    websocket.close()
  }
  
  function doSend()
  {
    if (sendMessage.value.length > 511 || sendMessage.value.length == 0)
    {
      sendMessage.value = "";
      return;
    }
    //logToConsole("SENT: " + sendMessage.value);
    var bb = new ByteBuffer();
    bb.LE(true);
    bb.writeShort(2); //SV_CHAT_MSG is 2
    bb.writeUTF8String(sendMessage.value);
    bb.writeByte(0); //null terminate  *shakes fist at javascript*
    websocket.send(bb.toArrayBuffer());

    sendMessage.value = "";

  }

  function logToConsole(message)
  {
    var pre = document.createElement("p");
    pre.style.wordWrap = "break-word";
    pre.innerHTML = getSecureTag()+message;
    consoleLog.appendChild(pre);

    while (consoleLog.childNodes.length > 10)
    {
      consoleLog.removeChild(consoleLog.firstChild);
    }
    
    consoleLog.scrollTop = consoleLog.scrollHeight;
  }

  function sendUsername()
  {
    var bb = new ByteBuffer();
    bb.LE=true;
    //bb.writeShort(4 + wsUsername.value.length);
    bb.writeShort(0); //username msg is 0
    bb.writeUTF8String(wsUsername.value);
    bb.writeByte(0); //null terminate  *shakes fist at javascript*
    websocket.send(bb.toArrayBuffer());
  }
  
  function onOpen(evt)
  {
    logToConsole("Connected, sending username: " + wsUsername.value);
    setGuiConnected(true);
    sendUsername();
  }
  
  function onClose(evt)
  {
    logToConsole("DISCONNECTED");
    setGuiConnected(false);
  }
  
  function onMessage(evt)
  {
    var bb = ByteBuffer.wrap(evt.data, true); //set little endian as true,  grrr
    bb.LE = true; //little endian
    var eventId = bb.readShort();

    if (eventId == 0)
    {
      logToConsole('login acknowledged');
    }
    else if (eventId == 1)
    {
      var len = evt.data.byteLength - 2; //this accounts for 2 bytes
      var msg = bb.readUTF8StringBytes(len);
      logToConsole(msg);
    }
    
    //logToConsole('msg eventId(' + eventId +')' + '</span>');
  }

  function onError(evt)
  {
    logToConsole('<span style="color: red;">ERROR:</span> ' + evt.data);
  }
  
  function setGuiConnected(isConnected)
  {
    wsUri.disabled = isConnected;
    connectBut.disabled = isConnected;
    disconnectBut.disabled = !isConnected;
    sendMessage.disabled = !isConnected;
    sendBut.disabled = !isConnected;
    var labelColor = "black";
    if (isConnected)
    {
      labelColor = "#999999";
    }
    
  }
	
	function clearLog()
	{
		while (consoleLog.childNodes.length > 0)
		{
			consoleLog.removeChild(consoleLog.lastChild);
		}
	}
	
	function getSecureTag()
	{
			return '';
	}
  
  window.addEventListener("load", echoHandlePageLoad, false);
