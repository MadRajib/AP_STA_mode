var colorPicker = new iro.ColorPicker("#picker", {
    // Set the size of the color picker
    width: 320,
    // Set the initial color to pure red
    color: "#000"
});

colorPicker.on('color:change', function(color) {
    // log the current color as a HEX string
    post_data(hexToRgb(color.hexString));
});

let last_color  = {r:0,g:0,b:0};
function hexToRgb(hex) {
    // Expand shorthand form (e.g. "03F") to full form (e.g. "0033FF")
    var shorthandRegex = /^#?([a-f\d])([a-f\d])([a-f\d])$/i;
    hex = hex.replace(shorthandRegex, function(m, r, g, b) {
      return r + r + g + g + b + b;
    });
  
    var result = /^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(hex);
    return result ? {
      r: parseInt(result[1], 16),
      g: parseInt(result[2], 16),
      b: parseInt(result[3], 16)
    } : null;
} 
function distance(last, current){
    let sm = Math.abs(last.r - current.r) + Math.abs(last.g - current.g) + Math.abs(last.b - current.b)
    return sm/3.0;  
}
function post_data(data) {
    console.log(distance(last_color,data));
    if(distance(last_color,data) == 0 ) 
        return;
    last_color = data;
    const payload = {
        "R" : data.r.toString(),
        "G" : data.g.toString(),
        "B" : data.b.toString(),    
    };
    console.log(payload);
      fetch("http://192.168.1.22:80/led",{
        method:'POST',
        mode: 'no-cors',
        headers: {
          'Content-Type': 'application/json'
        },
        body: JSON.stringify(payload)
      }).then(function(res) {
        // return res.json()
        return res;
      })
      .then(function(data) {
        // console.log(data);
      })
      .catch(err => {
        console.log(err);
      });
}