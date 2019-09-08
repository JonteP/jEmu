function sine_generator()
ENV_BITS = 10;
ENV_LEN = bitshift(1,ENV_BITS);
ENV_STEP = 128/ENV_LEN;
SIN_BITS = 10;
SIN_LEN  = bitshift(1,SIN_BITS);
TL_RES_LEN = 256;
TL_TAB_LEN = (11*2*TL_RES_LEN);
sin_tab = [];

%x = 0..255, y = round(-log( sin( (x+0.5)*pi/256/2)) / log(2) * 256)
x=256;
m=[];
o=[];

for i = 0:255
    m(i+1) = sin((i+.5) * (pi/2)/x);
    o(i+1) = -log2(m(i+1)) * x;
end

end