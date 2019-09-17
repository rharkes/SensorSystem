classdef SensorSystem <handle
    %SENSORSYSTEM Summary of this class goes here
    %   Detailed explanation goes here
    
    properties
        port
        s
        Temperature
        RelativeHumidity
        AbsoluteHumidity
        CO2
        mylocation
        CO2setp
    end
    
    methods
        function obj = SensorSystem()
            [p,~,~] = fileparts(mfilename('fullpath'));obj.mylocation=p;
            obj.port=readconfig(fullfile(p,'config.ini'),'SensorSystem','port');
            obj.s = serial(obj.port,'Parity','none','BaudRate',9600,'DataBits',8,'StopBits',1,'FlowControl','none');
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
        function obj = setCO2(obj,setp)
            fprintf(obj.s,setp); %send
            obj.CO2setp = setp;
        end
        function obj = read(obj)
            %METHOD1 Summary of this method goes here
            %   Detailed explanation goes here
            fprintf(obj.s,'?'); %send
            while obj.s.BytesAvailable<11
                pause(0.01)
            end
            
            data = uint8(fread(obj.s,obj.s.BytesAvailable,'uint8'));
            if length(data)==11&&data(end)==10
                    obj = convertData(obj,data(1:end-1));
            else
                warning('incorrect data from SensorSystem:')
                fprintf(1,'%.0f\n',data)
            end
        end
        
        %% when obj deleted, close connection
        function delete(obj)
            fclose(obj.s);
        end
        function obj = convertData(obj,data)
            COtwo = typecast(data(1:4),'uint32');
            if COtwo==0,COtwo=nan;end
            T_Raw = typecast(data(5:6),'uint16');
            if T_Raw==0,T_Raw=nan;end
            RH_Raw = typecast(data(7:8),'uint16');
            if RH_Raw==0,RH_Raw=nan;end
            AH_Raw = typecast(data(9:10),'uint16');
            if AH_Raw==0,AH_Raw=nan;end
            obj.CO2 = double(COtwo);
            obj.RelativeHumidity = (double(RH_Raw) / 65535.0) * 100.0;
            obj.Temperature = (double(T_Raw) / 65535.00) * 175 - 45;
            obj.AbsoluteHumidity = double(AH_Raw)/2^8;
            
        end
        %% correct relative
        function AH = getAbsoluteHumidity(obj)
            eSat = 6.11*10^(7.5*obj.Temperature/(237.7+obj.Temperature));
            vp = eSat*(obj.RelativeHumidity/100);
            AH = 1000*vp*100/((obj.Temperature+273)*461.5);
        end
        function monitorALL(obj)
            t=datetime('now');
            CO2=nan;Temp=nan;RH=nan;
            %creat GUI
            f = figure(1);clf
            p1 = uipanel(f,'Title','Graph','Position',[.0 .0 .75 1]);
            p2 = uipanel(f,'Title','Current values','Position',[.75 .0 .25 1]);
            ax = axes(p1);
            subplot(3,1,1,ax);
            pl1=plot(t,CO2);ylabel('CO2')
            pl1.Parent.XTickLabel={};
            subplot(3,1,2);
            pl2=plot(t,Temp);ylabel('Temperature')
            pl2.Parent.XTickLabel={};
            subplot(3,1,3);
            pl3=plot(t,RH);ylabel('Relative Humidity')
            
            txt1 = uicontrol(p2,'Style','text','String','CO2: 5%','Units','normalized','Position',[0,0,1,.85],'HorizontalAlignment','left');
            txt2 = uicontrol(p2,'Style','text','String','Temp: 37°C','Units','normalized','Position',[0,0,1,.55],'HorizontalAlignment','left');
            txt3 = uicontrol(p2,'Style','text','String','RH: 5%','Units','normalized','Position',[0,0,1,.25],'HorizontalAlignment','left');
            closeButton = uicontrol(p2,'Style','pushbutton','String','STOP','Units','normalized','Position',[0.05,0.05,0.5,0.1]);
            closeButton.Callback = @closeButtonPushed;
            
            function closeButtonPushed(src,event)
                src.UserData='stop';
                src.String = 'STOPPED';
            end
            
            fprintf(obj.s,'monitor on'); %send
            while isempty(closeButton.UserData)
                while obj.s.BytesAvailable<11 %wait for data
                    pause(0.01)
                end
                data = uint8(fread(obj.s,obj.s.BytesAvailable,'uint8'));
                if length(data)==11&&data(end)==10
                    obj = convertData(obj,data(1:end-1));
                else
                    warning('incorrect data from SensorSystem:')
                    fprintf(1,'%.0f\n',data)
                end
                txt1.String=sprintf('CO2: %.1f%%',obj.CO2/1E4);
                txt2.String=sprintf('Temp: %.1f°C',obj.Temperature);
                txt3.String=sprintf('RH: %.1f%%',obj.RelativeHumidity);
                t(end+1) = datetime('now');
                CO2(end+1)=obj.CO2/1E4;
                Temp(end+1)=obj.Temperature;
                RH(end+1)=obj.RelativeHumidity;
                pl1.XData=t;pl1.YData=CO2;
                pl2.XData=t;pl2.YData=Temp;
                pl3.XData=t;pl3.YData=RH;
            end
            fprintf(obj.s,'debug off'); %send
        end
        function monitorCO2(obj)
            fprintf(obj.s,'monitor on'); %send
            f = figure(1);clf;
            t=datetime([],[],[]);CO2=[];
            xlabel('Time');
            ylabel('CO2 (ppm)')
            while isvalid(f)
                while obj.s.BytesAvailable<11
                    pause(0.01)
                end
                data = uint8(fread(obj.s,obj.s.BytesAvailable,'uint8'));
                if length(data)==11&&data(end)==10
                    obj = convertData(obj,data(1:end-1));
                else
                    warning('incorrect data from SensorSystem:')
                    fprintf(1,'%.0f\n',data)
                end
                t(end+1) = datetime('now');
                CO2(end+1)=obj.CO2;
                plot(t,CO2);
            end
            figure(1);clf;
            plot(t,CO2);
            fprintf(obj.s,'debug off'); %send
        end
    end
end

