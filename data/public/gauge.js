/*
var gaugePower = new RadialGauge({
    renderTo: 'power-gauge',
    width: 300,
    height: 300,
    units: "Power (W)",
    minValue: 0,
    maxValue: 1000,
    colorValueBoxRect: "#049faa",
    colorValueBoxRectEnd: "#049faa",
    colorValueBoxBackground: "#f1fbfc",
    valueInt: 2,
    majorTicks: [
        "0",
        "20",
        "40",
        "60",
        "80",
        "100"
  
    ],
    minorTicks: 4,
    strokeTicks: true,
    highlights: [
        {
            "from": 80,
            "to": 100,
            "color": "#03C0C1"
        }
    ],
    colorPlate: "#fff",
    borderShadowWidth: 0,
    borders: false,
    needleType: "line",
    colorNeedle: "#007F80",
    colorNeedleEnd: "#007F80",
    needleWidth: 2,
    needleCircleSize: 3,
    colorNeedleCircleOuter: "#007F80",
    needleCircleOuter: true,
    needleCircleInner: false,
    animationDuration: 1500,
    animationRule: "linear"
  }).draw();
*/
var powerGauge = new RadialGauge({    
    renderTo: 'power-gauge',
    units: "Power (W)",
    minValue: 0,
    maxValue: 6000,
    majorTicks: [
        "0",
        "500",
        "1500",
        "2000",
        "2500",
        "3000",
        "3500",
        "4000",
        "4500",
        "5000",
        "5500",
        "6000"
    ]
}).draw();

  var ws = new WebSocket("wss://" + document.location.host + "/live");
  ws.onmessage = function(e){
    values = e.data.toString().split('\n');
    if(values.length>0){
        values.pop();
        values = values.map((v,i)=>parseFloat(v));
        //values = values.map((v,i)=>{ return {y:parseFloat(v),x:count++}});
        //powerField.innerText = values[values.length-1].y.toFixed(2);
        //addData(values);
        var val = values.shift();
        console.log(val);
        powerGauge.value = val;
        //gaugePower.update();

        if(window.ondata){
            window.ondata(values);
        }
    }

}