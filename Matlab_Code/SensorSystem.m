classdef SensorSystem
    %SENSORSYSTEM Summary of this class goes here
    %   Detailed explanation goes here
    
    properties
        port
        s
        Temperature
        RelativeHumidity
        AbsoluteHumidity
        CO2
        LM
        mylocation
    end
    
    methods
        function obj = SensorSystem()
            [p,~,~] = fileparts(mfilename('fullpath'));obj.mylocation=p;
            obj.port=readconfig(fullfile(p,'config.ini'),'SensorSystem','port');
            obj.s = serial(obj.port,'Parity','none','BaudRate',115200,'DataBits',8,'StopBits',1,'FlowControl','none');
            obj.Temperature = nan; %degrees Celsius
            obj.RelativeHumidity = nan; % percent
            obj.CO2 = nan; %ppm
            obj.AbsoluteHumidity = nan; %g/m3
            fopen(obj.s);
            while ~strcmp(obj.s.Status,'open')&&strcmp(obj.s.TransferStatus,'idle')
                pause(0.01)
            end
            pause(2);
            fprintf(obj.s,'*IDN?'); %send
            while obj.s.BytesAvailable<2
                pause(0.01)
            end
            ID = fscanf(obj.s);
            if ~strcmp(ID,sprintf('SensorSystem\r\n'))
                error('SensorSystem not found');
            end
        end
        
        function obj = read(obj)
            %METHOD1 Summary of this method goes here
            %   Detailed explanation goes here
            fprintf(obj.s,'?'); %send
            while obj.s.BytesAvailable<8
                pause(0.01)
            end
            
            data = uint8(fread(obj.s,obj.s.BytesAvailable,'uint8'));
            if length(data)==8
                COtwo = typecast(data(1:2),'uint16');
                if COtwo==0,COtwo=nan;end
                T_Raw = typecast(data(3:4),'uint16');
                if T_Raw==0,T_Raw=nan;end
                RH_Raw = typecast(data(5:6),'uint16');
                if RH_Raw==0,RH_Raw=nan;end
                AH_Raw = typecast(data(7:8),'uint16');
                if AH_Raw==0,AH_Raw=nan;end
                obj.CO2 = double(COtwo);
                obj.RelativeHumidity = (double(RH_Raw) / 65535.0) * 100.0;
                obj.Temperature = (double(T_Raw) / 65535.00) * 175 - 45;
                obj.AbsoluteHumidity = double(AH_Raw)/2^8;
            else
                warning('incorrect data from SensorSystem:')
                fprintf(1,'%.0f\n',data)
            end
        end
        
        %% when obj deleted, close connection
        function delete(obj)
            fclose(obj.s);
        end
        %% correct relative 
    end
end

