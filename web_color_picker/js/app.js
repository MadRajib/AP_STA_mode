var colorPicker = new iro.ColorPicker("#picker", {
  // Set the size of the color picker
  width: 320,
  // Set the initial color to pure red
  color: "#000"
});

colorPicker.on('color:change', function (color) {
  // log the current color as a HEX string
  post_data(hexToRgb(color.hexString), document.getElementById("patterns").value.toLowerCase());
});


let last_color = {
  r: 0,
  g: 0,
  b: 0
};

function hexToRgb(hex) {
  // Expand shorthand form (e.g. "03F") to full form (e.g. "0033FF")
  var shorthandRegex = /^#?([a-f\d])([a-f\d])([a-f\d])$/i;
  hex = hex.replace(shorthandRegex, function (m, r, g, b) {
    return r + r + g + g + b + b;
  });

  var result = /^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(hex);
  return result ? {
    r: parseInt(result[1], 16),
    g: parseInt(result[2], 16),
    b: parseInt(result[3], 16)
  } : null;
}

function distance(last, current) {
  let sm = Math.abs(last.r - current.r) + Math.abs(last.g - current.g) + Math.abs(last.b - current.b)
  return sm / 3.0;
}

function post_data(data, pattern) {
  console.log(distance(last_color, data));

  last_color = data;
  const payload = {
    "R": data.r.toString(),
    "G": data.g.toString(),
    "B": data.b.toString(),
    "pattern": pattern,
  };
  console.log(payload);
  fetch("http://192.168.69.154:80/led", {
      method: 'POST',
      mode: 'no-cors',
      headers: {
        'Content-Type': 'application/json'
      },
      body: JSON.stringify(payload)
    }).then(function (res) {
      // return res.json()
      return res;
    })
    .then(function (data) {
      // console.log(data);
    })
    .catch(err => {
      console.log(err);
    });
}

function setPattern(event) {
  var x = document.getElementById("patterns");
  post_data(hexToRgb(colorPicker.color.hexString), x.value.toLowerCase());
}

document.getElementById("patterns").addEventListener("change", setPattern);

function powerOff() {
  post_data({
    "r": '0',
    "g": '0',
    "b": '0'
  }, "static");
}
document.getElementById("power-btn").addEventListener("click", powerOff);

function settings() {
  let element = document.getElementById("div-picker");
  let config_div = document.getElementById("config");
  let hidden = element.getAttribute("hidden");

  if (hidden) {
    element.removeAttribute("hidden");
    config_div.setAttribute("hidden", "hidden");
  } else {
    config_div.removeAttribute("hidden");
    element.setAttribute("hidden", "hidden");
  }
}

document.getElementById("settings-btn").addEventListener("click", settings);

function logSubmit(event) {
  event.preventDefault();

  const payload = {
    "mqtt": document.getElementById("server").value.toString(),
    "ssid": document.getElementById("ssid").value.toString(),
    "psswd": document.getElementById("psswd").value.toString(),
  };

  console.log(payload);
  fetch("http://192.168.4.1/config", {
      method: 'POST',
      mode: 'no-cors',
      headers: {
        'Content-Type': 'application/json'
      },
      body: JSON.stringify(payload)
    }).then(function (res) {
      // return res.json()
      return res;
    })
    .then(function (data) {
      console.log(data);
    })
    .catch(err => {
      console.log(err);
    });
}

const form = document.getElementById('config_form');
form.addEventListener('submit', logSubmit);