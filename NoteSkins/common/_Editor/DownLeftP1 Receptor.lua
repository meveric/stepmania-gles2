--If a Command has "NOTESKIN:GetMetricA" in it, that means it gets the command from the metrics.ini, else use function(self) end; to define command.
--If you dont know how "NOTESKIN:GetMetricA" works here is an explanation.
--NOTESKIN:GetMetricA("The [Group] in the metrics.ini", "The actual Command to fallback on in the metrics.ini");

local t = Def.ActorFrame {
	Def.Sprite {
		Texture=NOTESKIN:GetPath( "_downleftp1", "Tap Note" );
		Frame0000=99;
		Delay0000=1;
		InitCommand=function(self)
			self:glowblink();
			self:effectcolor1(0.4, 0.4, 0.4, 0.4);
			self:effectcolor2(0.8, 0.8, 0.8, 0.4);
			self:effectclock('beat');
			self:effecttiming(0.2, 0, 0.8, 0);
		end;
		NoneCommand=NOTESKIN:GetMetricA("ReceptorArrow", "NoneCommand");
		PressCommand=NOTESKIN:GetMetricA("ReceptorArrow", "PressCommand");
		LiftCommand=NOTESKIN:GetMetricA("ReceptorArrow", "LiftCommand");
		W5Command=NOTESKIN:GetMetricA("ReceptorArrow", "W5Command");
		W4Command=NOTESKIN:GetMetricA("ReceptorArrow", "W4Command");
		W3Command=NOTESKIN:GetMetricA("ReceptorArrow", "W3Command");
		W2Command=NOTESKIN:GetMetricA("ReceptorArrow", "W2Command");
		W1Command=NOTESKIN:GetMetricA("ReceptorArrow", "W1Command");
	};
};
return t;
