api_version = '0.1';
monitor_names = [];
monitor_desc = {};
current_page = "home-view";

eiger = {
    api_version: '0.1',
    current_page: '.home-view',
    status: {}
};


String.prototype.replaceAll = function(search, replacement) {
    var target = this;
    return target.replace(new RegExp(search, 'g'), replacement);
};

$.put = function(url, data, callback, type)
{
  if ( $.isFunction(data) ){
    type = type || callback,
    callback = data,
    data = {}
  }

  return $.ajax({
    url: url,
    type: 'PUT',
    dataType: 'json',
    data: JSON.stringify(data),
    headers: {'Content-Type': 'application/json',
              'Accept': 'application/json'},
    success: callback,
    contentType: type
  });
}


$( document ).ready(function() 
{
  update_api_version();
  update_api_adapters();
  update_server_setup();
  //render('#/home-view');
  render(decodeURI(window.location.hash));

  //setInterval(update_server_setup, 1000);
  setInterval(update_api_read_status, 100);


  // Configuration items
  $('#set-exposure').on('change', function(event){
    write_exposure();
  });
  $('#set-period').on('change', function(event){
    write_acq_period();
  });
  $('#set-nimages').on('change', function(event){
    write_nimages();
  });

  $(window).on('hashchange', function(){
		// On every hash change the render function is called with the new hash.
		// This is how the navigation of the app is executed.
		render(decodeURI(window.location.hash));
	});
});

function write_exposure()
{
    val = $('#set-exposure').val();
    $.put('/api/' + api_version + '/eiger/detector/api/1.6.0/config/count_time', parseFloat(val), function(response){});
}

function write_acq_period()
{
    val = $('#set-period').val();
    $.put('/api/' + api_version + '/eiger/detector/api/1.6.0/config/frame_time', parseFloat(val), function(response){});
}

function write_nimages()
{
    val = $('#set-nimages').val();
    $.put('/api/' + api_version + '/eiger/detector/api/1.6.0/config/nimages', parseInt(val), function(response){});
}

function process_cmd_response(response)
{
}

function update_api_version() {

    $.getJSON('/api', function(response) {
        $('#api-version').html(response.api);
        eiger.api_version = response.api;
    });
}

function update_api_adapters() {
//alert("HERE");
    $.getJSON('/api/' + api_version + '/adapters/', function(response) {
        adapter_list = response.adapters.join(", ");
        $('#api-adapters').html(adapter_list);
        //alert(adapter_list);
    });
}

/*function update_server_setup() {
    $.getJSON('/api/' + api_version + '/eiger/driver/', function(response) {
        $('#server-start-time').html(response.start_time);
        $('#server-up-time').html(response.up_time);
        $('#server-username').html(response.username);
        $('#server-hw-address').html(response.hardware.address);
        $('#server-hw-port').html(response.hardware.port);
        if (response.hardware.connected){
            connected = 1;
            $('#server-hw-reconnect').hide();
            $('#server-hw-connected').html(led_html(1, "green", 25));
        } else {
            connected = 0;
            $('#server-hw-reconnect').show();
            $('#server-hw-connected').html(led_html(1, "red", 25));
        }
    });
}*/

function update_api_read_status()
{
  $.getJSON('/api/' + api_version + '/eiger/detector/api/1.6.0/config', function(response) {
//    var detector = response['detector'];

//    alert(JSON.stringify(response));

    $('#get-exposure').html(Math.round(response['count_time']['value'] * 1000) / 1000);
    $('#get-period').html(Math.round(response['frame_time']['value'] * 1000) / 1000);
    $('#get-nimages').html(response['nimages']['value']);



    //    $('#det-image-counter').html(detector['Image_counter']);
//    $('#det-acq-counter').html(detector['Acquisition_counter']);
//    $('#det-train-number').html(detector['Train_number']);


//    $('#det-master-reset').html(led_html(detector['Master_reset'],'green', 20));
  });


  $.getJSON('/api/' + api_version + '/eiger/detector/api/1.6.0/status', function(response) {
    //    var detector = response['detector'];
    
    //    alert(JSON.stringify(response));
    
        $('#get-acquire').html(led_html(response['acquisition_complete']['value'],'green', 20));
    
    
    
        //    $('#det-image-counter').html(detector['Image_counter']);
    //    $('#det-acq-counter').html(detector['Acquisition_counter']);
    //    $('#det-train-number').html(detector['Train_number']);
    
    
    //    $('#det-master-reset').html(led_html(detector['Master_reset'],'green', 20));
      });
    }

function led_html(value, colour, width)
{
  var html_text = "<img width=" + width + "px src=img/";
  if (value == 0){
    html_text += "led-off";
  } else {
    html_text +=  colour + "-led-on";
  }
  html_text += ".png></img>";
  return html_text;
}


function render(url) 
{
  // This function decides what type of page to show 
  // depending on the current url hash value.
  // Get the keyword from the url.
  var temp = "." + url.split('/')[1];
  if (url.split('/')[1]){
    document.title = "Eiger (" + url.split('/')[1] + ")";
  } else {
    document.title = "Eiger";
  }
  // Hide whatever page is currently shown.
  $(".page").hide();
		
  // Show the new page
  $(temp).show();
  current_page = temp;
    
  if (temp == ".home-view"){
    update_api_version();
    update_api_adapters();
  }
}
