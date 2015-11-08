-- Align left when cropping to 4:3.
local Color1 = color(Var "Color1");

local t = Def.ActorFrame {
	LoadActor(Var "File1") .. {
		OnCommand=function(self)
			self:x(SCREEN_CENTER_X);
			self:y(SCREEN_CENTER_Y);
			self:scale_or_crop_background();
			self:diffuse(Color1);
			self:effectclock("music");
		end;
		GainFocusCommand=function(self)
			self:play();
		end;
		LoseFocusCommand=function(self)
			self:pause();
		end;
	};
};

return t;
