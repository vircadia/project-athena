var Metaverse = {
  accessToken: null
}

var CURRENT_METAVERSE_URL;

var currentStepNumber;

$(document).ready(function(){
  getMetaverseUrl(function(metaverse_url) {
    CURRENT_METAVERSE_URL = metaverse_url;

    Strings.ADD_PLACE_NOT_CONNECTED_MESSAGE = "You must have an access token to query your Metaverse places.<br><br>" +
      "Please go back and connect your account.";

    $('#connect-account-btn').attr('href', CURRENT_METAVERSE_URL + "/user/tokens/new?for_domain_server=true");

    $('[data-toggle="tooltip"]').tooltip();

    $('.perms-link').on('click', function() {
      var modal_body = '<div>';
      modal_body += '<b>None</b> - No one will have permissions. Only you and the users your have given administrator privileges to will have permissions.</br></br>';
      modal_body += '<b>Friends</b> - Users who are your Friends in the Metaverse.</br></br>';
      modal_body += '<b>Users logged into the Metaverse</b> - Users who are currently logged into the Metaverse.</br></br>';
      modal_body += '<b>Everyone</b> - Anyone who uses the Metaverse.';
      modal_body += '</div>';

      dialog = bootbox.dialog({
        title: "User definition",
        message: modal_body,
        closeButton: true
      });
      return false;
    });

    $('body').on('click', '.next-button', function() {
      goToNextStep();
    });

    $('body').on('click', '.back-button', function() {
      goToPreviousStep();
    });

    $('body').on('click', '#skip-wizard-button', function() {
      skipWizard();
    })

    $('body').on('click', '#connect-account-btn', function() {
      $(this).blur();
      prepareAccessTokenPrompt(function(accessToken) {
        Metaverse.accessToken = accessToken;
        saveAccessToken();
        promptToCreateDomainID();
      });
    });
  
  $('body').on('click', '#save-permissions', function() {
    savePermissions();
  });
  
  $('body').on('click', '#save-threading-settings', function() {
    saveThreadingSettings();
  });

    function triggerSaveUsernamePassword(event) {
        if (event.keyCode === 13) {
            $("#save-username-password").click();
        }
    }
    $("#http_username").keyup(triggerSaveUsernamePassword);
    $("#http_password").keyup(triggerSaveUsernamePassword);
    $("#verify_http_password").keyup(triggerSaveUsernamePassword);
    $('body').on('click', '#save-username-password', function() {
      saveUsernamePassword();
    });

    $('body').on('click', '#change-place-name', function() {
      chooseFromMetaversePlaces(Settings.data.values.metaverse.access_token, "/0,-10,0", function(placeName) {
        updatePlaceNameLink(placeName);
      });
    });

    $('body').on('click', '#visit-domain', function() {
      $('#share-link')[0].click();
    });

    $('input[type=radio][name=connect-radio]').change(function() {
      var inputs = $('input[type=radio][name=rez-radio]');
      var disabled = [];

      switch (this.value) {
        case 'none':
          disabled = inputs.splice(1);
          break;
        case 'friends':
          disabled = inputs.splice(2);
          break;
        case 'logged-in':
          disabled = inputs.splice(3);
          break;
        case 'everyone':
          disabled = inputs.splice(4);
          break;
      }

      $.each(inputs, function() {
        $(this).prop('disabled', false);
      });
      $.each(disabled, function() {
        if ($(this).prop('checked')) {
          $(inputs.last()).prop('checked', true);
        }
        $(this).prop('disabled', true);
      });
    });

    reloadSettings(function(success) {
      if (success) {
        getDomainFromAPI();
        setupWizardSteps();
        updatePlaceNameDisplay();
        updateUsernameDisplay();
      } else {
        swal({
          title: '',
          type: 'error',
          text: "There was a problem loading the domain settings.\nPlease refresh the page to try again.",
        });
      }
    });
  });
});

function copyToClipboard(element) {
  var $temp = $("<input>");
  $("body").append($temp);
  $temp.val($(element).text()).select();
  document.execCommand("copy");
  $temp.remove();
}

function setupWizardSteps() {
  currentStepNumber = Settings.data.values.wizard.steps_completed;
  var steps = null;

  if (Settings.data.values.wizard.cloud_domain) {
    $('.desktop-only').remove();
    $('.wizard-step:first').find('.back-button').hide();

    steps = $('.wizard-step');
    $(steps).each(function(i) {
      $(this).children(".step-title").text("Step " + (i + 1) + " of " + (steps.length - 1));
    });

    $('#permissions-description').html('You <span id="username-display"></span>have been assigned administrator privileges to this domain.');
    $('#admin-description').html('Add more Metaverse usernames');
  } else {
    $('.cloud-only').remove();
    $('#save-threading-settings').text("Finish");

    steps = $('.wizard-step');
    $(steps).each(function(i) {
      $(this).children(".step-title").text("Step " + (i + 1) + " of " + steps.length);
    });

    if (currentStepNumber == 0) {
      $('#skip-wizard-button').show();
    }
  }

  var currentStep = steps[currentStepNumber];
  $(currentStep).show();
}

function promptToCreateDomainID() {
  setTimeout(function () {
    createDomainIDPrompt(function (label) {
      var domainJSON = {
        "label": label
      };

      $.post("/api/domains", domainJSON, function (data) {
        if (data.status === "failure") {
          swal.showInputError("Error: " + data.error);
          return;
        }
        
        swal.close();

        // we successfully created a domain ID, set it on that field
        var domainID = data.domain.domainId;
        console.log("Setting domain ID to ", data, domainID);

        var formJSON = {
          "metaverse": {
            "automatic_networking": "full",
            "id": domainID
          },
          "descriptors": {
              "world_name": label
          }
        };

        // POST the form JSON to the domain-server settings.json endpoint so the settings are saved
        postSettings(formJSON, goToNextStep);
      }, 'json').fail(function (data) {
        if (data && data.status === "failure") {
          swal.showInputError("Error: " + data.error);
        } else {
          swal.showInputError("Error: Failed to post to metaverse.");
        }
        console.log("Failed to create domain ID...");
      });
    });
  }, 500); // Apparently swal needs time before opening another prompt.
}

function updatePlaceNameLink(address) {
  if (address) {
    var url = URLs.PLACE_URL + '/' + address;
    $('#place-name-link').html('Your domain is reachable at: <a target="_blank" href="' + url + '">' + address + '</a>');
    $('#share-field a').attr('href', url).text(url);
  }
}

function updatePlaceNameDisplay() {
  if (Settings.data.values.metaverse.id) {
    $.getJSON(CURRENT_METAVERSE_URL + '/api/v1/domains/' + Settings.data.values.metaverse.id, function(data) {

      if (data.status === 'success') {
        if (data.domain.default_place_name) {
          // Place name
          updatePlaceNameLink(data.domain.default_place_name);
        } else if (data.domain.name) {
          // Temporary name
          updatePlaceNameLink(data.domain.name);
        } else if (data.domain.network_address) {
          if (data.domain.network_port !== 40102) {
            // IP:PORT
            updatePlaceNameLink(data.domain.network_address + ':' + data.domain.network_port);
          } else {
            // IP
            updatePlaceNameLink(data.domain.network_address);
          }
        }
      } else {
        console.warn('Request Failed');
      }
    }).fail(function() {
      console.warn('Request Failed');
    });
  } else {
    console.warn('No metaverse domain ID!');
  }
}

function updateUsernameDisplay() {
  var permissions = Settings.data.values.security.permissions;
  if (permissions.length > 0) {
    $('#username-display').html('<b>(' + permissions[0].permissions_id + ')</b> ');
  }
}

function reloadSettings(callback) {
  $.getJSON('/settings.json', function(data){
    Settings.data = data;

    if (callback) {
      // call the callback now that settings are loaded
      callback(true);
    }
  }).fail(function() {
    if (callback) {
      // call the failure object since settings load failed
      callback(false)
    }
  });
}

function postSettings(jsonSettings, callback) {
  console.log("----- SAVING ------");
  console.log(JSON.stringify(jsonSettings));

  // POST the form JSON to the domain-server settings.json endpoint so the settings are saved
  $.ajax('/settings.json', {
    data: JSON.stringify(jsonSettings),
    contentType: 'application/json',
    type: 'POST'
  }).done(function(data){
    if (data.status == "success") {
      if (callback) {
        callback();
      }
      reloadSettings();
    } else {
      swal("Error", Strings.LOADING_SETTINGS_ERROR)
      reloadSettings();
    }
  }).fail(function(){
    swal("Error", Strings.LOADING_SETTINGS_ERROR)
    reloadSettings();
  });
}

function goToNextStep() {
  $('#skip-wizard-button').hide();

  var currentStep = $('body').find('.wizard-step:visible');
  var nextStep = currentStep.next('.wizard-step');

  if (nextStep.length > 0) {
    currentStep.hide();
    nextStep.show();

    currentStepNumber += 1;

    postSettings({
      "wizard": {
        "steps_completed": currentStepNumber.toString()
      }
    });
  } else {
    postSettings({
      "wizard": {
        "steps_completed": "0",
        "completed_once": true
      }
    }, redirectToSettings);
  }
}

function goToPreviousStep() {
  var currentStep = $('body').find('.wizard-step:visible');
  var previousStep = currentStep.prev('.wizard-step');

  var formJSON = {
    "wizard": {}
  }

  if (previousStep.length > 0) {
    currentStep.hide();
    previousStep.show();

    currentStepNumber -= 1;

    postSettings({
      "wizard": {
        "steps_completed": currentStepNumber.toString()
      }
    });
  }
}

function skipWizard() {
  postSettings({
    "wizard": {
      "steps_completed": "0",
      "completed_once": true
    }
  }, redirectToSettings);
}

function redirectToSettings() {
  var redirectURL = "/settings" + location.search;
  if (Settings.data.values.wizard.cloud_domain) {
    if (location.search.length > 0) {
      redirectURL += "&";
    } else {
      redirectURL += "?";
    }
    redirectURL += "cloud-wizard-exit";
  }
  location.href = redirectURL;
}

function saveAccessToken() {
  var formJSON = {
    "metaverse": {}
  }
  if (Metaverse.accessToken) {
    formJSON.metaverse.access_token = Metaverse.accessToken;
  }

  // remove focus from the button
  $(this).blur();

  // POST the form JSON to the domain-server settings.json endpoint so the settings are saved
  postSettings(formJSON);
}

function getSettingDescriptionForKey(groupKey, settingKey) {
  for (var i in Settings.data.descriptions) {
    var group = Settings.data.descriptions[i];
    if (group.name === groupKey) {
      for (var j in group.settings) {
        var setting = group.settings[j];
        if (setting.name === settingKey) {
          return setting;
        }
      }
    }
  }
}

function savePermissions() {
  var localhostPermissions = (Settings.data.values.wizard.cloud_domain !== true)
  var anonymousCanConnect = false;
  var friendsCanConnect = false;
  var loggedInCanConnect = false;
  var anonymousCanRez = false;
  var friendsCanRez = false;
  var loggedInCanRez = false;

  var connectValue = $('input[name=connect-radio]:checked').val();
  var rezValue = $('input[name=rez-radio]:checked').val();

  switch (connectValue) {
    case "friends":
      friendsCanConnect = true;
      break;
    case "logged-in":
      friendsCanConnect = true;
      loggedInCanConnect = true;
      break;
    case "everyone":
      anonymousCanConnect = true;
      friendsCanConnect = true;
      loggedInCanConnect = true;
      break;
  }

  switch (rezValue) {
    case "friends":
      friendsCanRez = true;
      break;
    case "logged-in":
      friendsCanRez = true;
      loggedInCanRez = true;
      break;
    case "everyone":
      anonymousCanRez = true;
      friendsCanRez = true;
      loggedInCanRez = true;
      break;
  }

  var admins = $('#admin-usernames').val().split(',');

  if (Settings.data.values.security.permissions) {
    var existingAdmins = Settings.data.values.security.permissions.map(function(value) {
      return value.permissions_id;
    });
    admins = admins.concat(existingAdmins);
  }

  // Filter out unique values
  admins = _.uniq(admins.map(function(username) {
    return username.trim();
  })).filter(function(username) {
    return username !== "";
  });

  var formJSON = {
    "security": {
      "standard_permissions": [
        {
          "id_can_connect": anonymousCanConnect,
          "id_can_rez_avatar_entities": anonymousCanConnect,
          "id_can_rez": anonymousCanRez,
          "id_can_rez_tmp": anonymousCanRez,
          "permissions_id": "anonymous"
        },
        {
          "id_can_connect": friendsCanConnect,
          "id_can_rez_avatar_entities": friendsCanConnect,
          "id_can_rez": friendsCanRez,
          "id_can_rez_tmp": friendsCanRez,
          "permissions_id": "friends"
        },
        {
          "id_can_connect": loggedInCanConnect,
          "id_can_rez_avatar_entities": loggedInCanConnect,
          "id_can_rez": loggedInCanRez,
          "id_can_rez_tmp": loggedInCanRez,
          "permissions_id": "logged-in"
        },
        {
          "id_can_adjust_locks": localhostPermissions,
          "id_can_connect": localhostPermissions,
          "id_can_rez_avatar_entities": localhostPermissions,
          "id_can_connect_past_max_capacity": localhostPermissions,
          "id_can_kick": localhostPermissions,
          "id_can_replace_content": localhostPermissions,
          "id_can_rez": localhostPermissions,
          "id_can_rez_tmp": localhostPermissions,
          "id_can_write_to_asset_server": localhostPermissions,
          "permissions_id": "localhost"
        }
      ]
    }
  }

  if (admins.length > 0) {
    formJSON.security.permissions = [];

    var permissionsDesc = getSettingDescriptionForKey("security", "permissions");
    for (var i in admins) {
      var admin = admins[i];
      var perms = {};
      for (var i in permissionsDesc.columns) {
        var name = permissionsDesc.columns[i].name;
        if (name === "permissions_id") {
          perms[name] = admin;
        } else {
          perms[name] = true;
        }
      }
      formJSON.security.permissions.push(perms);
    }
  }

  // remove focus from the button
  $(this).blur();

  postSettings(formJSON, goToNextStep);
}

function saveUsernamePassword() {
  var username = $("#http_username").val();
  var password = $("#http_password").val();
  var verify_password = $("#verify_http_password").val();

  if (username.length == 0) {
    bootbox.alert({ "message": "You must set a username!", "title": "Username Error" });
    return;
  }

  if (password.length == 0) {
    bootbox.alert({ "message": "You must set a password!", "title": "Password Error" });
    return;
  }

  if (password != verify_password) {
    bootbox.alert({ "message": "Passwords must match!", "title": "Password Error" });
    return;
  }

  currentStepNumber += 1;

  var formJSON = {
    "security": {
      "http_username": username,
      "http_password": sha256_digest(password)
    },
    "wizard": {
      "steps_completed": currentStepNumber.toString()
    }
  }

  // remove focus from the button
  $(this).blur();

  // POST the form JSON to the domain-server settings.json endpoint so the settings are saved
  postSettings(formJSON, goToNextStep);
}

function saveThreadingSettings() {
    var enable_automatic_threading = $("#enable-automatic-threading").prop("checked");
    
    currentStepNumber += 1;
  
    var formJSON = {
      "audio_threading": {
        "auto_threads": enable_automatic_threading
      },
      "avatar_mixer": {
        "auto_threads": enable_automatic_threading
      },
      "wizard": {
        "steps_completed": currentStepNumber.toString()
      }
    }
  
    // remove focus from the button
    $(this).blur();
  
    // POST the form JSON to the domain-server settings.json endpoint so the settings are saved
    postSettings(formJSON, goToNextStep);
}
