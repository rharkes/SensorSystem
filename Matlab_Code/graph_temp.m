if~isempty(instrfind),fclose(instrfind);end %close all serial ports 
SS=SensorSystem();
while (any(isnan([SS.CO2,SS.RelativeHumidity,SS.Temperature])))
    SS=SS.read();
    pause(0.5);
end
t=datetime('now');
CO2 = SS.CO2;
RH = SS.RelativeHumidity;
Te = SS.Temperature;
AH = SS.AbsoluteHumidity;
f =figure(1);clf;
while isvalid(f)
    subplot(4,1,1);plot(t,CO2);title(sprintf('CO2 (%.2f %%)',CO2(end)))
    subplot(4,1,2);plot(t,RH);title(sprintf('RH (%.1f %%)',RH(end)))
    subplot(4,1,3);plot(t,Te);title(sprintf('Temperature(%.2f °C)',Te(end)))
    subplot(4,1,4);plot(t,AH);title(sprintf('AH(%.2f g/m3)',AH(end)))
    pause(1);
    SS=SS.read();
    t(end+1)=datetime('now');
    CO2(end+1) = SS.CO2;
    RH(end+1) = SS.RelativeHumidity;
    Te(end+1) = SS.Temperature;
    AH(end+1) = SS.AbsoluteHumidity;
end
delete(SS)