local player = Var "Player"
local blinkTime = 1.2
local barWidth = 256;
local barHeight = 32;
local c;
local LifeMeter, MaxLives, CurLives;
local LifeRatio;

local t = Def.ActorFrame {
	LoadActor("_lives")..{
		InitCommand=function(self)
			self:pause();
			self:horizalign(left);
			self:x(barWidth / -2);
		end;
		BeginCommand=function(self,param)
			local screen = SCREENMAN:GetTopScreen();
			local glifemeter = screen:GetLifeMeter(player);
				self:setstate(glifemeter:GetTotalLives()-1);
				
				if glifemeter:GetTotalLives() <= 4 then
					self:zoomx(barWidth/(4*64));
				else
					self:zoomx(barWidth/((glifemeter:GetTotalLives())*64));
				end
				self:cropright((640-(((glifemeter:GetTotalLives())*64)))/640);
		end;
		LifeChangedMessageCommand=function(self,param)
			if param.Player == player then
				if param.LivesLeft == 0 then
					self:visible(false)
				else
					self:setstate( math.max(param.LivesLeft-1,0) )
					self:visible(true)
				end
			end
		end;
		StartCommand=function(self,param)
			if param.Player == player then
				self:setstate( math.max(param.LivesLeft-1,0) );			
			end			
		end;
		FinishCommand=function(self)
			self:playcommand("Start");
		end;
	};
};

return t;