var xhrRequest = function (url, type, callback) {
  var xhr = new XMLHttpRequest();
  xhr.onload = function () {
    console.log(this.responseText);
    callback(this.responseText);
  };
  xhr.open(type, url);
  xhr.send();
};



function locationSuccess(pos) {
  // Construct URL
  var url = "http://finance.google.com/finance/info?client=ig&q=SXX";
      
//      "http://api.openweathermap.org/data/2.5/weather?lat=" +
//      pos.coords.latitude + "&lon=" + pos.coords.longitude + '&appid=';
      
//  url = "http://www.nactem.ac.uk/software/acromine/dictionary.py?sf=BMI";
  
  console.log('LOOKING UP...');
  console.log(url);

  // Send request to OpenWeatherMap
  xhrRequest(url, 'GET', 
    function(responseText) {
      console.log('LOOKED UP...');
      console.log(responseText);
      console.log('...');
      
      responseText = responseText.substring(3);
      
//      responseText = "{ \"stock\" : " + responseText + "}";
      
      console.log(responseText);
      console.log('...');

      // responseText contains a JSON object with weather info
      var json = JSON.parse(responseText);
      console.log('parsed ' + json);
      console.log('parsed ' + json[0].t);

      // Temperature in Kelvin requires adjustment
      var temperature = json[0].t;
      console.log('Ticker is ' + temperature);

      // Conditions
      var conditions = json[0].l;      
      console.log('Conditions are ' + conditions);

      // Assemble dictionary using our keys
      var dictionary = {
        'KEY_TEMPERATURE': temperature,
        'KEY_CONDITIONS': conditions
      };

      // Send to Pebble
      Pebble.sendAppMessage(dictionary,
      function(e) {
        console.log('Weather info sent to Pebble successfully!');
      },
      function(e) {
        console.log('Error sending weather info to Pebble!');
        }
      );
    }      
  );
}

function locationError(err) {
  console.log("Error requesting location!");
}

function getWeather() {
  navigator.geolocation.getCurrentPosition(
    locationSuccess,
    locationError,
    {timeout: 15000, maximumAge: 60000}
  );
}

// Listen for when the watchface is opened
Pebble.addEventListener('ready', 
  function(e) {
    console.log("PebbleKit JS ready!");

    // Get the initial weather
    getWeather();
  }
);

// Listen for when an AppMessage is received
Pebble.addEventListener('appmessage',
  function(e) {
    console.log("AppMessage received!");
    getWeather();
  }                     
);
