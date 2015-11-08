local t = Def.ActorFrame{}


t[#t+1] = LoadActor("Center_blend")..{
	InitCommand=function(self)
		self:diffuseshift();
		self:effectcolor1(color("#9b873766"));
		self:effectcolor2(color("#9b873766"));
	end;
}

t[#t+1] = LoadActor("Center_blend")..{
	InitCommand=function(self)
		self:blend(Blend.Add);
		self:diffuseshift();
		self:effectcolor1(color("#ccb752FF"));
		self:effectcolor2(color("#ccb75266"));
		self:effectclock("bgm");
		self:effecttiming(1, 0, 0, 0);
	end;
}

t[#t+1] = LoadActor("Center_blend")..{
	InitCommand=function(self)
		self:diffuseshift();
		self:effectcolor1(color("#ccb752FF"));
		self:effectcolor2(color("#ccb752FF"));
		self:fadetop(1);
	end;
}

t[#t+1] = LoadActor("Center_fill")..{
	InitCommand=function(self)
		self:blend(Blend.Add);
		self:diffuseshift();
		self:effectcolor1(color("#ccb752FF"));
		self:effectcolor2(color("#ccb752FF"));
	end;
}

t[#t+1] = LoadActor("Center_feet")..{
	InitCommand=function(self)
		self:blend(Blend.Add);
		self:diffusealpha(0.6);
	end;
}


t[#t+1] = LoadActor("Center border");


return t