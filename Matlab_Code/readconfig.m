function [ out ] = readconfig( filename,header,value )
%READCONFIG reads a configuration file. output = -1 if value not found
%   Configfile is in acii-format:
% {header}
% value = anything

fid=fopen(filename,'rt');
l=fgetl(fid);
h='';
out=-1;
while ~isnumeric(l)
    if regexp(l,'{.*}')
        h=l(2:end-1);
        v={'',''};
    else
        l = regexprep(l,' = | =|= ','='); %remove space around = sign
        v = regexp(l,'=','split');
    end
    if strcmp(v{1},value)&&strcmp(h,header)
        out=v{2};
        l=-1;
    else
        l=fgetl(fid);
    end
end
fclose(fid);