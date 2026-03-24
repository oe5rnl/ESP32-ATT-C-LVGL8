

## Display am Pico
Die Anbindung erfolgt 12C

| I2C | Display | Pico       |
|-----|---------|------------|
| GND | GND     | GND Pin 18 | 
| 3V3 | VCC     | 3V3 Pin 36 |           
| SDA | SDA     | SDA Pin 19 |
| SCL | SCL     | SCL Pin 20 |



## GPIO Belegung

1  GPIO-0   Tx zum ESP
2  GPIO-1   Rx zum ESP
3  GND
4  GPIO-2   Mode-Select0
5  GPIO-3   Mode Select1
6  GPIO-4   SDA Display
7  GPIO-5   SCL Display
8  GND
9  GPIO-6   Relais-1 a
10 GPIO-7   Relais-1 b
11 GPIO-8   Relais-2 a
12 GPIO-9   Relais-2 b
13 GND
14 GPIO-10  Relais-3 a 
15 GPIO-11  Relais-3 b
16 GPIO-12  Relais-4 a
17 GPIO-13  Relais-4 b
18 GND
19 GPIO-14  Relais-5 a
20 GPIO-15  Relais-5 b

21 GPIO-16  Relais-6 a
22 GPIO-17  Relais-7 a
23 GND
24 GPIO-18  Relais-7 b
25 GPIO-19  Regler-Clk (A)  
26 GPIO-20  Regler-DT  (B) 
27 GPIO-21  Regler-SW  
28 GND      Regler-GND
29 GPIO-22  NC
30 RUN
31 GPIO-26  NC
32 GPIO-27  NC
33 GND
34 GPIO-28NC
35 ADC_REF
36 2,2V OUT
37 3V3_EN
38 GND
39 VSYS
40 VBUS
