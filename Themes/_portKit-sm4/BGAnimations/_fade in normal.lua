return Def.Quad {
	InitCommand=function(self)
		self:diffuse(color("#000000"));
		self:stretchto(SCREEN_LEFT, SCREEN_TOP, SCREEN_RIGHT, SCREEN_BOTTOM);
	end;
	StartTransitioningCommand=function(self)
		self:diffusealpha(1);
		self:linear(0.3);
		self:diffusealpha(0);
	end;
};
