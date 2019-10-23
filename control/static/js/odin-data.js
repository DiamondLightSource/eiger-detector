

function read_fr_status()
{
  $.getJSON('/api/' + api_version + '/fr/status/', function(response)
  {
      //alert(JSON.stringify(response['value']));
      fr_data = response['value'];
      for (index = 0; index < fr_data.length; index++){
          fr = fr_data[index];
//          alert(JSON.stringify(fr));
          fr_index = "fr" + (index+1);
//          alert(JSON.stringify('#' + fr_index + '-connected'));
          $('#' + fr_index + '-connected').html(led_html(fr['connected'],'green', 20));
          if (fr['connected'] === true || fr['connected'] === 'true'){
//              alert("Index " + index + " connected");
//              fr_index = "fr" + index+1;
//              $('#' + fr_index + '-connected').html(led_html(detector['safety_driven_assert_marker_out_3_completed'],'green', 20));
              populate_fr_table(fr_index, fr);
          } else {
              empty_fr_table(fr_index);
          }
      }
  });
}

function populate_fr_table(fr_index, fr)
{
    $('#' + fr_index + '-buffer-manager').html(led_html(fr['status']['buffer_manager_configured'],'green', 20));
    $('#' + fr_index + '-rx-thread').html(led_html(fr['status']['rx_thread_configured'],'green', 20));
    $('#' + fr_index + '-decoder').html(led_html(fr['status']['decoder_configured'],'green', 20));
    $('#' + fr_index + '-ipc').html(led_html(fr['status']['ipc_configured'],'green', 20));
    $('#' + fr_index + '-config-complete').html(led_html(fr['status']['configuration_complete'],'green', 20));
    $('#' + fr_index + '-empty-buffers').html(fr['buffers']['empty']);
    $('#' + fr_index + '-frames-received').html(fr['frames']['received']);
    $('#' + fr_index + '-frames-timed-out').html(fr['frames']['timedout']);
    $('#' + fr_index + '-frames-released').html(fr['frames']['released']);
}

function empty_fr_table(fr_index)
{
    $('#' + fr_index + '-buffer-manager').html("");
    $('#' + fr_index + '-rx-thread').html("");
    $('#' + fr_index + '-decoder').html("");
    $('#' + fr_index + '-ipc').html("");
    $('#' + fr_index + '-config-complete').html("");
    $('#' + fr_index + '-empty-buffers').html("");
    $('#' + fr_index + '-frames-received').html("");
    $('#' + fr_index + '-frames-timed-out').html("");
    $('#' + fr_index + '-frames-released').html("");
}

function read_fp_status()
{
  $.getJSON('/api/' + api_version + '/fp/status/', function(response)
  {
      //alert(JSON.stringify(response['value']));
      fp_data = response['value'];
      for (index = 0; index < fr_data.length; index++){
          fp = fp_data[index];
//          alert(JSON.stringify(fp));
          fp_index = "fp" + (index+1);
//          alert(JSON.stringify('#' + fr_index + '-connected'));
          $('#' + fp_index + '-connected').html(led_html(fp['connected'],'green', 20));
          if (fp['connected'] === true || fp['connected'] === 'true'){
//              alert("Index " + index + " connected");
//              fr_index = "fr" + index+1;
//              $('#' + fr_index + '-connected').html(led_html(detector['safety_driven_assert_marker_out_3_completed'],'green', 20));
              populate_fp_table(fp_index, fp);
          } else {
              empty_fp_table(fp_index);
          }
      }
  });
}

function populate_fp_table(fp_index, fp)
{
    $('#' + fp_index + '-shared-mem').html(led_html(fp['shared_memory']['configured'], 'green', 20));
    $('#' + fp_index + '-hdf-processes').html(fp['hdf']['processes']);
    $('#' + fp_index + '-hdf-rank').html(fp['hdf']['rank']);
    $('#' + fp_index + '-hdf-written').html(fp['hdf']['frames_written'] + " / " + fp['hdf']['frames_max']);
    $('#' + fp_index + '-hdf-file-path').html(fp['hdf']['file_name']);
    $('#' + fp_index + '-writing').html(led_html(fp['hdf']['writing'], 'green', 20));
}

function empty_fp_table(fp_index)
{
    $('#' + fp_index + '-shared-mem').html('');
    $('#' + fp_index + '-hdf-processes').html('');
    $('#' + fp_index + '-hdf-rank').html('');
    $('#' + fp_index + '-hdf-written').html('');
    $('#' + fp_index + '-hdf-file-path').html('');
    $('#' + fp_index + '-writing').html('');
}

function send_fp_command(command, data)
{
    $.ajax({
        url: '/api/' + api_version + '/fp/config/' + command,
        type: 'PUT',
        dataType: 'json',
        data: data,
        headers: {'Content-Type': 'application/json',
                  'Accept': 'application/json'},
        success: null,
        error: process_fp_error
    });
}

function send_fp_reset()
{
    $.ajax({
        url: '/api/' + api_version + '/fp/command/reset_statistics',
        type: 'PUT',
        dataType: 'json',
        headers: {'Content-Type': 'application/json',
                  'Accept': 'application/json'},
        success: null,
        error: process_fp_error
    });
}

function update_fp_params() {
    // Send the number of frames
    send_fp_command('hdf/frames', $('#set-fp-frames').val());
    // Send the path
    send_fp_command('hdf/file/path', $('#set-fp-path').val());
    // Send the name
    send_fp_command('hdf/file/name', $('#set-fp-filename').val());
}

function start_fp_writing()
{
    // Send the write true
    send_fp_reset();
    send_fp_command('hdf/master', 'data');
    send_fp_command('hdf/write', '1');
}

function stop_fp_writing()
{
    // Send the write false
    send_fp_command('hdf/write', '0');
}

function process_fp_error(response)
{
    alert("FAILED: " + response['responseJSON']['error']);
}

$(document).ready(function()
{
    setInterval(read_fr_status, 500);
    setInterval(read_fp_status, 500);
    $('#fp-start-cmd').on('click', function(event){
      start_fp_writing();
    });
    $('#fp-stop-cmd').on('click', function(event){
      stop_fp_writing();
    });
    $('#set-fp-frames').on('change', function(event){
      update_fp_params();
    });
    $('#set-fp-filename').on('change', function(event){
      update_fp_params();
    });
    $('#set-fp-path').on('change', function(event){
      update_fp_params();
    });
});
