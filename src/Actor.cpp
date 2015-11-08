#include "global.h"
#include "Actor.h"
#include "RageDisplay.h"
#include "RageUtil.h"
#include "RageMath.h"
#include "RageLog.h"
#include "arch/Dialog/Dialog.h"
#include "Foreach.h"
#include "XmlFile.h"
#include "LuaBinding.h"
#include "ThemeManager.h"
#include "LuaReference.h"
#include "MessageManager.h"
#include "LightsManager.h" // for NUM_CabinetLight
#include "ActorUtil.h"
#include "Preference.h"
#include <typeinfo>

static Preference<bool> g_bShowMasks("ShowMasks", false);

/**
 * @brief Set up a hidden Actor that won't be drawn.
 *
 * It's useful to be able to construct a basic Actor in XML, in
 * order to simply delay a Transition, or receive and send broadcasts.
 * Since these actors will never draw, set them hidden by default. */
class HiddenActor: public Actor
{
public:
	HiddenActor() { SetVisible(false); }
	virtual HiddenActor *Copy() const;
};
REGISTER_ACTOR_CLASS_WITH_NAME( HiddenActor, Actor );

float Actor::g_fCurrentBGMTime = 0, Actor::g_fCurrentBGMBeat;
float Actor::g_fCurrentBGMTimeNoOffset = 0, Actor::g_fCurrentBGMBeatNoOffset = 0;
vector<float> Actor::g_vfCurrentBGMBeatPlayer(NUM_PlayerNumber, 0);
vector<float> Actor::g_vfCurrentBGMBeatPlayerNoOffset(NUM_PlayerNumber, 0);


Actor *Actor::Copy() const { return new Actor(*this); }

static float g_fCabinetLights[NUM_CabinetLight];

static const char *HorizAlignNames[] = {
	"Left",
	"Center",
	"Right"
};
XToString( HorizAlign );
LuaXType( HorizAlign );

static const char *VertAlignNames[] = {
	"Top",
	"Middle",
	"Bottom"
};
XToString( VertAlign );
LuaXType( VertAlign );

void Actor::SetBGMTime( float fTime, float fBeat, float fTimeNoOffset, float fBeatNoOffset )
{
	g_fCurrentBGMTime = fTime;
	g_fCurrentBGMBeat = fBeat;

	/* This timer is generally only used for effects tied to the background music
	 * when GameSoundManager is aligning music beats.  Alignment doesn't handle
	 * g_fVisualDelaySeconds. */
	g_fCurrentBGMTimeNoOffset = fTimeNoOffset;
	g_fCurrentBGMBeatNoOffset = fBeatNoOffset;
}

void Actor::SetPlayerBGMBeat( PlayerNumber pn, float fBeat, float fBeatNoOffset )
{
	g_vfCurrentBGMBeatPlayer[pn] = fBeat;
	g_vfCurrentBGMBeatPlayerNoOffset[pn] = fBeatNoOffset;
}

void Actor::SetBGMLight( int iLightNumber, float fCabinetLights )
{
	ASSERT( iLightNumber < NUM_CabinetLight );
	g_fCabinetLights[iLightNumber] = fCabinetLights;
}

void Actor::InitState()
{
	StopTweening();

	m_pTempState = NULL;

	m_baseRotation = RageVector3( 0, 0, 0 );
	m_baseScale = RageVector3( 1, 1, 1 );
	m_fBaseAlpha = 1;
	m_internalDiffuse = RageColor( 1, 1, 1, 1 );
	m_internalGlow = RageColor( 0, 0, 0, 0 );

	m_start.Init();
	m_current.Init();

	m_fHorizAlign = 0.5f;
	m_fVertAlign = 0.5f;
#if defined(SSC_FUTURES)
	for( unsigned i = 0; i < m_Effects.size(); ++i )
		m_Effects[i] = no_effect;
#else
	m_Effect =  no_effect;
#endif
	m_fSecsIntoEffect = 0;
	m_fEffectDelta = 0;
	m_fEffectRampUp = 0.5f;
	m_fEffectHoldAtHalf = 0;
	m_fEffectRampDown = 0.5f;
	m_fEffectHoldAtZero = 0;
	m_fEffectOffset = 0;
	m_EffectClock = CLOCK_TIMER;
	m_vEffectMagnitude = RageVector3(0,0,10);
	m_effectColor1 = RageColor(1,1,1,1);
	m_effectColor2 = RageColor(1,1,1,1);

	m_bVisible = true;
	m_fShadowLengthX = 0;
	m_fShadowLengthY = 0;
	m_ShadowColor = RageColor(0,0,0,0.5f);
	m_bIsAnimating = true;
	m_fHibernateSecondsLeft = 0;
	m_iDrawOrder = 0;

	m_bTextureWrapping = false;
	m_bTextureFiltering = true;

	m_BlendMode = BLEND_NORMAL;
	m_fZBias = 0;
	m_bClearZBuffer = false;
	m_ZTestMode = ZTEST_OFF;
	m_bZWrite = false;
	m_CullMode = CULL_NONE;
}

static bool GetMessageNameFromCommandName( const RString &sCommandName, RString &sMessageNameOut )
{
	if( sCommandName.Right(7) == "Message" )
	{
		sMessageNameOut = sCommandName.Left(sCommandName.size()-7);
		return true;
	}
	else
	{
		return false;
	}
}

Actor::Actor()
{
	m_pLuaInstance = new LuaClass;
	Lua *L = LUA->Get();
		m_pLuaInstance->PushSelf( L );
		lua_newtable( L );
		lua_pushvalue( L, -1 );
		lua_setmetatable( L, -2 );
		lua_setfield( L, -2, "ctx" );
		lua_pop( L, 1 );
	LUA->Release( L );
	
	m_size = RageVector2( 1, 1 );
	InitState();
	m_pParent = NULL;
	m_bFirstUpdate = true;
}

Actor::~Actor()
{
	StopTweening();
	UnsubscribeAll();
}

Actor::Actor( const Actor &cpy ):
	MessageSubscriber( cpy )
{
	/* Don't copy an Actor in the middle of rendering. */
	ASSERT( cpy.m_pTempState == NULL );
	m_pTempState = NULL;

#define CPY(x) x = cpy.x
	CPY( m_sName );
	CPY( m_pParent );
	CPY( m_pLuaInstance );

	CPY( m_baseRotation );
	CPY( m_baseScale );
	CPY( m_fBaseAlpha );
	CPY( m_internalDiffuse );
	CPY( m_internalGlow );


	CPY( m_size );
	CPY( m_current );
	CPY( m_start );
	for( unsigned i = 0; i < cpy.m_Tweens.size(); ++i )
		m_Tweens.push_back( new TweenStateAndInfo(*cpy.m_Tweens[i]) );

	CPY( m_bFirstUpdate );

	CPY( m_fHorizAlign );
	CPY( m_fVertAlign );
#if defined(SSC_FUTURES)
	// I'm a bit worried about this -aj
	for( unsigned i = 0; i < cpy.m_Effects.size(); ++i )
		m_Effects.push_back( (*cpy.m_Effects[i]) );
#else
	CPY( m_Effect );
#endif
	CPY( m_fSecsIntoEffect );
	CPY( m_fEffectDelta );
	CPY( m_fEffectRampUp );
	CPY( m_fEffectHoldAtHalf );
	CPY( m_fEffectRampDown );
	CPY( m_fEffectHoldAtZero );
	CPY( m_fEffectOffset );
	CPY( m_EffectClock );

	CPY( m_effectColor1 );
	CPY( m_effectColor2 );
	CPY( m_vEffectMagnitude );

	CPY( m_bVisible );
	CPY( m_fHibernateSecondsLeft );
	CPY( m_fShadowLengthX );
	CPY( m_fShadowLengthY );
	CPY( m_ShadowColor );
	CPY( m_bIsAnimating );
	CPY( m_iDrawOrder );

	CPY( m_bTextureWrapping );
	CPY( m_bTextureFiltering );
	CPY( m_BlendMode );
	CPY( m_bClearZBuffer );
	CPY( m_ZTestMode );
	CPY( m_bZWrite );
	CPY( m_fZBias );
	CPY( m_CullMode );

	CPY( m_mapNameToCommands );
#undef CPY
}

/* XXX: This calls InitCommand, which must happen after all other
 * initialization (eg. ActorFrame loading children).  However, it
 * also loads input variables, which should happen first.  The
 * former is more important. */
void Actor::LoadFromNode( const XNode* pNode )
{
	Lua *L = LUA->Get();
	FOREACH_CONST_Attr( pNode, pAttr )
	{
		// Load Name, if any.
		const RString &sKeyName = pAttr->first;
		const XNodeValue *pValue = pAttr->second;
		if( sKeyName == "Name" )			SetName( pValue->GetValue<RString>() );
		else if( sKeyName == "BaseRotationX" )		SetBaseRotationX( pValue->GetValue<float>() );
		else if( sKeyName == "BaseRotationY" )		SetBaseRotationY( pValue->GetValue<float>() );
		else if( sKeyName == "BaseRotationZ" )		SetBaseRotationZ( pValue->GetValue<float>() );
		else if( sKeyName == "BaseZoomX" )		SetBaseZoomX( pValue->GetValue<float>() );
		else if( sKeyName == "BaseZoomY" )		SetBaseZoomY( pValue->GetValue<float>() );
		else if( sKeyName == "BaseZoomZ" )		SetBaseZoomZ( pValue->GetValue<float>() );
		else if( EndsWith(sKeyName,"Command") )
		{
			LuaReference *pRef = new LuaReference;
			pValue->PushValue( L );
			pRef->SetFromStack( L );
			RString sCmdName = sKeyName.Left( sKeyName.size()-7 );
			AddCommand( sCmdName, apActorCommands( pRef ) );
		}
	}

	LUA->Release( L );

	// Don't recurse Init.  It gets called once for every Actor when the 
	// Actor is loaded, and we don't want to call it again.
	PlayCommandNoRecurse( Message("Init") );
}

void Actor::Draw()
{
	if( !m_bVisible )
		return;	// early abort
	if( m_fHibernateSecondsLeft > 0 )
		return;	// early abort
	if( this->EarlyAbortDraw() )
		return;

	// call the most-derived versions
	this->BeginDraw();	
	this->DrawPrimitives();	// call the most-derived version of DrawPrimitives();
	this->EndDraw();
}

void Actor::BeginDraw()		// set the world matrix and calculate actor properties
{
	DISPLAY->PushMatrix();	// we're actually going to do some drawing in this function	

	// Somthing below may set m_pTempState to tempState
	m_pTempState = &m_current;

	// set temporary drawing properties based on Effects
	static TweenState tempState;

	// todo: account for SSC_FUTURES -aj
	if( m_Effect == no_effect )
	{
	}
	else
	{
		m_pTempState = &tempState;
		tempState = m_current;

		const float fTotalPeriod = GetEffectPeriod();
		ASSERT( fTotalPeriod > 0 );
		const float fTimeIntoEffect = fmodfp( m_fSecsIntoEffect+m_fEffectOffset, fTotalPeriod );

		float fPercentThroughEffect;
		if( fTimeIntoEffect < m_fEffectRampUp )
		{
			fPercentThroughEffect = SCALE( 
				fTimeIntoEffect, 
				0, 
				m_fEffectRampUp, 
				0.0f, 
				0.5f );
		}
		else if( fTimeIntoEffect < m_fEffectRampUp + m_fEffectHoldAtHalf )
		{
			fPercentThroughEffect = 0.5f;
		}
		else if( fTimeIntoEffect < m_fEffectRampUp + m_fEffectHoldAtHalf + m_fEffectRampDown )
		{
			fPercentThroughEffect = SCALE( 
				fTimeIntoEffect, 
				m_fEffectRampUp + m_fEffectHoldAtHalf, 
				m_fEffectRampUp + m_fEffectHoldAtHalf + m_fEffectRampDown, 
				0.5f, 
				1.0f );
		}
		else
		{
			fPercentThroughEffect = 0;
		}
		ASSERT_M( fPercentThroughEffect >= 0 && fPercentThroughEffect <= 1, 
			ssprintf("PercentThroughEffect: %f", fPercentThroughEffect) );

		bool bBlinkOn = fPercentThroughEffect > 0.5f;
		float fPercentBetweenColors = RageFastSin( (fPercentThroughEffect + 0.25f) * 2 * PI ) / 2 + 0.5f;
		ASSERT_M( fPercentBetweenColors >= 0 && fPercentBetweenColors <= 1,
			ssprintf("PercentBetweenColors: %f, PercentThroughEffect: %f", fPercentBetweenColors, fPercentThroughEffect) );
		float fOriginalAlpha = tempState.diffuse[0].a;

		// todo: account for SSC_FUTURES -aj
		switch( m_Effect )
		{
		case diffuse_blink:
			/* XXX: Should diffuse_blink and diffuse_shift multiply the tempState color? 
			 * (That would have the same effect with 1,1,1,1, and allow tweening the diffuse
			 * while blinking and shifting.) */
			for(int i=0; i<4; i++)
			{
				tempState.diffuse[i] = bBlinkOn ? m_effectColor1 : m_effectColor2;
				tempState.diffuse[i].a *= fOriginalAlpha;	// multiply the alphas so we can fade even while an effect is playing
			}
			break;
		case diffuse_shift:
			for(int i=0; i<4; i++)
			{
				tempState.diffuse[i] = m_effectColor1*fPercentBetweenColors + m_effectColor2*(1.0f-fPercentBetweenColors);
				tempState.diffuse[i].a *= fOriginalAlpha;	// multiply the alphas so we can fade even while an effect is playing
			}
			break;
		case diffuse_ramp:
			for(int i=0; i<4; i++)
			{
				tempState.diffuse[i] = m_effectColor1*fPercentThroughEffect + m_effectColor2*(1.0f-fPercentThroughEffect);
				tempState.diffuse[i].a *= fOriginalAlpha;	// multiply the alphas so we can fade even while an effect is playing
			}
			break;
		case glow_blink:
			tempState.glow = bBlinkOn ? m_effectColor1 : m_effectColor2;
			tempState.glow.a *= fOriginalAlpha;	// don't glow if the Actor is transparent!
			break;
		case glow_shift:
			tempState.glow = m_effectColor1*fPercentBetweenColors + m_effectColor2*(1.0f-fPercentBetweenColors);
			tempState.glow.a *= fOriginalAlpha;	// don't glow if the Actor is transparent!
			break;
		case glow_ramp:
			tempState.glow = m_effectColor1*fPercentThroughEffect + m_effectColor2*(1.0f-fPercentThroughEffect);
			tempState.glow.a *= fOriginalAlpha;	// don't glow if the Actor is transparent!
			break;
		case rainbow:
			tempState.diffuse[0] = RageColor(
				RageFastCos( fPercentBetweenColors*2*PI ) * 0.5f + 0.5f,
				RageFastCos( fPercentBetweenColors*2*PI + PI * 2.0f / 3.0f ) * 0.5f + 0.5f,
				RageFastCos( fPercentBetweenColors*2*PI + PI * 4.0f / 3.0f) * 0.5f + 0.5f,
				fOriginalAlpha );
			for( int i=1; i<4; i++ )
				tempState.diffuse[i] = tempState.diffuse[0];
			break;
		case wag:
			tempState.rotation += m_vEffectMagnitude * RageFastSin( fPercentThroughEffect * 2.0f * PI );
			break;
		case spin:
			// nothing needs to be here
			break;
		case vibrate:
			tempState.pos.x += m_vEffectMagnitude.x * randomf(-1.0f, 1.0f) * GetZoom();
			tempState.pos.y += m_vEffectMagnitude.y * randomf(-1.0f, 1.0f) * GetZoom();
			tempState.pos.z += m_vEffectMagnitude.z * randomf(-1.0f, 1.0f) * GetZoom();
			break;
		case bounce:
			{
				float fPercentOffset = RageFastSin( fPercentThroughEffect*PI ); 
				tempState.pos += m_vEffectMagnitude * fPercentOffset;
				tempState.pos.x = roundf( tempState.pos.x );
				tempState.pos.y = roundf( tempState.pos.y );
				tempState.pos.z = roundf( tempState.pos.z );
			}
			break;
		case bob:
			{
				float fPercentOffset = RageFastSin( fPercentThroughEffect*PI*2 ); 
				tempState.pos += m_vEffectMagnitude * fPercentOffset;
				tempState.pos.x = roundf( tempState.pos.x );
				tempState.pos.y = roundf( tempState.pos.y );
				tempState.pos.z = roundf( tempState.pos.z );
			}
			break;
		case pulse:
			{
				float fMinZoom = m_vEffectMagnitude[0];
				float fMaxZoom = m_vEffectMagnitude[1];
				float fPercentOffset = RageFastSin( fPercentThroughEffect*PI ); 
				float fZoom = SCALE( fPercentOffset, 0.f, 1.f, fMinZoom, fMaxZoom );
				tempState.scale *= fZoom;

				// Use the color as a Vector3 to scale the effect for added control
				RageColor c = SCALE( fPercentOffset, 0.f, 1.f, m_effectColor1, m_effectColor2 );
				tempState.scale.x *= c.r;
				tempState.scale.y *= c.g;
				tempState.scale.z *= c.b;
			}
			break;
		default:
			FAIL_M(ssprintf("Invalid effect: %i", m_Effect));
		}
	}

	if( m_fBaseAlpha != 1 )
		m_internalDiffuse.a *= m_fBaseAlpha;

	if( m_internalDiffuse != RageColor(1, 1, 1, 1) )
	{
		if( m_pTempState != &tempState )
		{
			m_pTempState = &tempState;
			tempState = m_current;
		}

		for( int i=0; i<4; i++ )
		{
			tempState.diffuse[i] *= m_internalDiffuse;
		}
		m_internalDiffuse = RageColor(1, 1, 1, 1);
	}

	if( m_internalGlow.a > 0 )
	{
		if( m_pTempState != &tempState )
		{
			m_pTempState = &tempState;
			tempState = m_current;
		}

		// Blend using Screen mode
		tempState.glow = tempState.glow + m_internalGlow - m_internalGlow * tempState.glow;
		m_internalGlow.a = 0;
	}

	if( m_pTempState->pos.x != 0 || m_pTempState->pos.y != 0 || m_pTempState->pos.z != 0 )	
	{
		RageMatrix m;
		RageMatrixTranslate( 
			&m, 
			m_pTempState->pos.x,
			m_pTempState->pos.y,
			m_pTempState->pos.z
			);
		DISPLAY->PreMultMatrix( m );
	}

	{
		/* The only time rotation and quat should normally be used simultaneously
		 * is for m_baseRotation. Most objects aren't rotated at all, so optimize
		 * that case. */
		const float fRotateX = m_pTempState->rotation.x + m_baseRotation.x;
		const float fRotateY = m_pTempState->rotation.y + m_baseRotation.y;
		const float fRotateZ = m_pTempState->rotation.z + m_baseRotation.z;

		if( fRotateX != 0 || fRotateY != 0 || fRotateZ != 0 )	
		{
			RageMatrix m;
			RageMatrixRotationXYZ( &m, fRotateX, fRotateY, fRotateZ );
			DISPLAY->PreMultMatrix( m );
		}
	}

	// handle scaling
	{
		const float fScaleX = m_pTempState->scale.x * m_baseScale.x;
		const float fScaleY = m_pTempState->scale.y * m_baseScale.y;
		const float fScaleZ = m_pTempState->scale.z * m_baseScale.z;

		if( fScaleX != 1 || fScaleY != 1 || fScaleZ != 1 )
		{
			RageMatrix m;
			RageMatrixScale( 
				&m,
				fScaleX,
				fScaleY,
				fScaleZ );
			DISPLAY->PreMultMatrix( m );
		}
	}

	// handle alignment; most actors have default alignment.
	if( unlikely(m_fHorizAlign != 0.5f || m_fVertAlign != 0.5f) )
	{
		float fX = SCALE( m_fHorizAlign, 0.0f, 1.0f, +m_size.x/2.0f, -m_size.x/2.0f );
		float fY = SCALE( m_fVertAlign, 0.0f, 1.0f, +m_size.y/2.0f, -m_size.y/2.0f );
		RageMatrix m;
		RageMatrixTranslate( 
			&m, 
			fX,
			fY,
			0
			);
		DISPLAY->PreMultMatrix( m );
	}

	if( m_pTempState->quat.x != 0 ||  m_pTempState->quat.y != 0 ||  m_pTempState->quat.z != 0 || m_pTempState->quat.w != 1 )
	{
		RageMatrix mat;
		RageMatrixFromQuat( &mat, m_pTempState->quat );

		DISPLAY->MultMatrix(mat);
	}

	// handle skews
	if( m_pTempState->fSkewX != 0 )
	{
		DISPLAY->SkewX( m_pTempState->fSkewX );
	}

	if( m_pTempState->fSkewY != 0 )
	{
		DISPLAY->SkewY( m_pTempState->fSkewY );
	}

}

void Actor::SetGlobalRenderStates()
{
	// set Actor-defined render states
	if( !g_bShowMasks.Get() || m_BlendMode != BLEND_NO_EFFECT )
		DISPLAY->SetBlendMode( m_BlendMode );
	DISPLAY->SetZWrite( m_bZWrite );
	DISPLAY->SetZTestMode( m_ZTestMode );

	// BLEND_NO_EFFECT is used to draw masks to the Z-buffer, which always wants
	// Z-bias enabled.
	if( m_fZBias == 0 && m_BlendMode == BLEND_NO_EFFECT )
		DISPLAY->SetZBias( 1.0f );
	else
		DISPLAY->SetZBias( m_fZBias );

	if( m_bClearZBuffer )
		DISPLAY->ClearZBuffer();
	DISPLAY->SetCullMode( m_CullMode );
}

void Actor::SetTextureRenderStates()
{
	DISPLAY->SetTextureWrapping( TextureUnit_1, m_bTextureWrapping );
	DISPLAY->SetTextureFiltering( TextureUnit_1, m_bTextureFiltering );
}

void Actor::EndDraw()
{
	DISPLAY->PopMatrix();
	m_pTempState = NULL;
}

void Actor::UpdateTweening( float fDeltaTime )
{
	while( 1 )
	{
		if( m_Tweens.empty() ) // nothing to do
			return;

		if( fDeltaTime == 0 )	// nothing will change
			return;

		// update current tween state
		// earliest tween
		TweenState &TS = m_Tweens[0]->state;
		TweenInfo  &TI = m_Tweens[0]->info;

		bool bBeginning = TI.m_fTimeLeftInTween == TI.m_fTweenTime;

		float fSecsToSubtract = min( TI.m_fTimeLeftInTween, fDeltaTime );
		TI.m_fTimeLeftInTween -= fSecsToSubtract;
		fDeltaTime -= fSecsToSubtract;

		RString sCommand = TI.m_sCommandName;
		if( bBeginning )			// we are just beginning this tween
			m_start = m_current;	// set the start position
	
		if( TI.m_fTimeLeftInTween == 0 )	// Current tween is over.  Stop.
		{
			m_current = TS;

			// delete the head tween
			delete m_Tweens.front();
			m_Tweens.erase( m_Tweens.begin() );
		}
		else	// in the middle of tweening. Recalcute the current position.
		{
			const float fPercentThroughTween = 1-(TI.m_fTimeLeftInTween / TI.m_fTweenTime);

			// distort the percentage if appropriate
			float fPercentAlongPath = TI.m_pTween->Tween( fPercentThroughTween );
			TweenState::MakeWeightedAverage( m_current, m_start, TS, fPercentAlongPath );
		}

		if( bBeginning )
		{
			// Execute the command in this tween (if any). Do this last, and don't
			// access TI or TS after, since this may modify the tweening queue.
			if( !sCommand.empty() )
			{
				if( sCommand.Left(1) == "!" )
					MESSAGEMAN->Broadcast( sCommand.substr(1) );
				else
					this->PlayCommand( sCommand );
			}
		}
	}
}

bool Actor::IsFirstUpdate() const
{
	return m_bFirstUpdate;
}

void Actor::Update( float fDeltaTime )
{
//	LOG->Trace( "Actor::Update( %f )", fDeltaTime );
	ASSERT_M( fDeltaTime >= 0, ssprintf("DeltaTime: %f",fDeltaTime) );

	if( m_fHibernateSecondsLeft > 0 )
	{
		m_fHibernateSecondsLeft -= fDeltaTime;
		if( m_fHibernateSecondsLeft > 0 )
			return;

		// Grab the leftover time.
		fDeltaTime = -m_fHibernateSecondsLeft;
		m_fHibernateSecondsLeft = 0;
	}

	this->UpdateInternal( fDeltaTime );
}

void Actor::UpdateInternal( float fDeltaTime )
{
	if( m_bFirstUpdate )
		m_bFirstUpdate = false;

	switch( m_EffectClock )
	{
	case CLOCK_TIMER:
		m_fSecsIntoEffect += fDeltaTime;
		m_fEffectDelta = fDeltaTime;

		/* Wrap the counter, so it doesn't increase indefinitely (causing loss
		 * of precision if a screen is left to sit for a day). */
		if( m_fSecsIntoEffect >= GetEffectPeriod() )
			m_fSecsIntoEffect -= GetEffectPeriod();
		break;

	case CLOCK_TIMER_GLOBAL:
	{
		float fTime = RageTimer::GetTimeSinceStartFast();
		m_fEffectDelta = fTime - m_fSecsIntoEffect;
		m_fSecsIntoEffect = fTime;
		break;
	}

	case CLOCK_BGM_BEAT:
		m_fEffectDelta = g_fCurrentBGMBeat - m_fSecsIntoEffect;
		m_fSecsIntoEffect = g_fCurrentBGMBeat;
		break;

	case CLOCK_BGM_BEAT_PLAYER1:
		m_fEffectDelta = g_vfCurrentBGMBeatPlayer[PLAYER_1] - m_fSecsIntoEffect;
		m_fSecsIntoEffect = g_vfCurrentBGMBeatPlayerNoOffset[PLAYER_1];
		break;
		
	case CLOCK_BGM_BEAT_PLAYER2:
		m_fEffectDelta = g_vfCurrentBGMBeatPlayer[PLAYER_2] - m_fSecsIntoEffect;
		m_fSecsIntoEffect = g_vfCurrentBGMBeatPlayerNoOffset[PLAYER_2];
		break;
	
	case CLOCK_BGM_TIME:
		m_fEffectDelta = g_fCurrentBGMTime - m_fSecsIntoEffect;
		m_fSecsIntoEffect = g_fCurrentBGMTime;
		break;

	case CLOCK_BGM_BEAT_NO_OFFSET:
		m_fEffectDelta = g_fCurrentBGMBeatNoOffset - m_fSecsIntoEffect;
		m_fSecsIntoEffect = g_fCurrentBGMBeatNoOffset;
		break;

	case CLOCK_BGM_TIME_NO_OFFSET:
		m_fEffectDelta = g_fCurrentBGMTimeNoOffset - m_fSecsIntoEffect;
		m_fSecsIntoEffect = g_fCurrentBGMTimeNoOffset;
		break;

	default:
		if( m_EffectClock >= CLOCK_LIGHT_1 && m_EffectClock <= CLOCK_LIGHT_LAST )
		{
			int i = m_EffectClock - CLOCK_LIGHT_1;
			m_fEffectDelta = g_fCabinetLights[i] - m_fSecsIntoEffect;
			m_fSecsIntoEffect = g_fCabinetLights[i];
		}
		break;
	}

	// update effect
	// todo: account for SSC_FUTURES -aj
	switch( m_Effect )
	{
		case spin:
			m_current.rotation += m_fEffectDelta*m_vEffectMagnitude;
			wrap( m_current.rotation.x, 360 );
			wrap( m_current.rotation.y, 360 );
			wrap( m_current.rotation.z, 360 );
			break;
		default: break;
	}

	UpdateTweening( fDeltaTime );
}

RString Actor::GetLineage() const
{
	RString sPath;
	
	if( m_pParent )
		sPath = m_pParent->GetLineage() + '/';
	sPath += ssprintf( "<%s> %s", typeid(*this).name(), m_sName.c_str() );
	return sPath;
}

void Actor::BeginTweening( float time, ITween *pTween )
{
	ASSERT( time >= 0 );

	time = max( time, 0 );

	// If the number of tweens to ever gets this large, there's probably an infinitely 
	// recursing ActorCommand.
	if( m_Tweens.size() > 50 )
	{
		RString sError = ssprintf( "Tween overflow: \"%s\"; infinitely recursing ActorCommand?", GetLineage().c_str() );

		LOG->Warn( "%s", sError.c_str() );
		Dialog::OK( sError );
		FinishTweening();
	}

	// add a new TweenState to the tail, and initialize it
	m_Tweens.push_back( new TweenStateAndInfo );

	// latest
	TweenState &TS = m_Tweens.back()->state;
	TweenInfo  &TI = m_Tweens.back()->info;

	if( m_Tweens.size() >= 2 )		// if there was already a TS on the stack
	{
		// initialize the new TS from the last TS in the list
		TS = m_Tweens[m_Tweens.size()-2]->state;
	}
	else
	{
		// This new TS is the only TS.
		// Set our tween starting and ending values to the current position.
		TS = m_current;
	}

	TI.m_pTween = pTween;
	TI.m_fTweenTime = time;
	TI.m_fTimeLeftInTween = time;
}

void Actor::BeginTweening( float time, TweenType tt )
{
	ITween *pTween = ITween::CreateFromType( tt );
	BeginTweening( time, pTween );
}

void Actor::StopTweening()
{
	for( unsigned i = 0; i < m_Tweens.size(); ++i )
		delete m_Tweens[i];
	m_Tweens.clear();
}

void Actor::FinishTweening()
{
	if( !m_Tweens.empty() )
		m_current = DestTweenState();
	StopTweening();
}

void Actor::HurryTweening( float factor )
{
	for( unsigned i = 0; i < m_Tweens.size(); ++i )
	{
		m_Tweens[i]->info.m_fTimeLeftInTween *= factor;
		m_Tweens[i]->info.m_fTweenTime *= factor;
	}
}

void Actor::ScaleTo( const RectF &rect, StretchType st )
{
	// width and height of rectangle
	float rect_width = rect.GetWidth();
	float rect_height = rect.GetHeight();

	if( rect_width < 0 )	SetRotationY( 180 );
	if( rect_height < 0 )	SetRotationX( 180 );

	// zoom fActor needed to scale the Actor to fill the rectangle
	float fNewZoomX = fabsf(rect_width  / m_size.x);
	float fNewZoomY = fabsf(rect_height / m_size.y);

	float fNewZoom = 0.f;
	switch( st )
	{
	case cover:
		fNewZoom = fNewZoomX>fNewZoomY ? fNewZoomX : fNewZoomY;	// use larger zoom
		break;
	case fit_inside:
		fNewZoom = fNewZoomX>fNewZoomY ? fNewZoomY : fNewZoomX; // use smaller zoom
		break;
	}

	SetX( rect.left + rect_width * m_fHorizAlign );
	SetY( rect.top + rect_height * m_fVertAlign );

	SetZoom( fNewZoom );
}

void Actor::SetEffectClockString( const RString &s )
{
	if     (s.EqualsNoCase("timer"))	this->SetEffectClock( CLOCK_TIMER );
	if     (s.EqualsNoCase("timerglobal"))	this->SetEffectClock( CLOCK_TIMER_GLOBAL );
	else if(s.EqualsNoCase("beat"))		this->SetEffectClock( CLOCK_BGM_BEAT );
	else if(s.EqualsNoCase("music"))	this->SetEffectClock( CLOCK_BGM_TIME );
	else if(s.EqualsNoCase("bgm"))		this->SetEffectClock( CLOCK_BGM_BEAT ); // compat, deprecated
	else if(s.EqualsNoCase("musicnooffset"))this->SetEffectClock( CLOCK_BGM_TIME_NO_OFFSET );
	else if(s.EqualsNoCase("beatnooffset"))	this->SetEffectClock( CLOCK_BGM_BEAT_NO_OFFSET );
	else
	{
		CabinetLight cl = StringToCabinetLight( s );
		if( cl == CabinetLight_Invalid )
			FAIL_M(ssprintf("Invalid cabinet light: %s", s.c_str()));

		this->SetEffectClock( (EffectClock) (cl + CLOCK_LIGHT_1) );
	}
}

void Actor::StretchTo( const RectF &r )
{
	// width and height of rectangle
	float width = r.GetWidth();
	float height = r.GetHeight();

	// center of the rectangle
	float cx = r.left + width/2.0f;
	float cy = r.top  + height/2.0f;

	// zoom fActor needed to scale the Actor to fill the rectangle
	float fNewZoomX = width  / m_size.x;
	float fNewZoomY = height / m_size.y;

	SetXY( cx, cy );
	SetZoomX( fNewZoomX );
	SetZoomY( fNewZoomY );
}


void Actor::SetEffectPeriod( float fTime )
{
	ASSERT( fTime > 0 );
	m_fEffectRampUp = fTime/2;
	m_fEffectHoldAtHalf = 0;
	m_fEffectRampDown = fTime/2;
	m_fEffectHoldAtZero = 0;
}

float Actor::GetEffectPeriod() const
{
	return m_fEffectRampUp + m_fEffectHoldAtHalf + m_fEffectRampDown + m_fEffectHoldAtZero;
}

void Actor::SetEffectTiming( float fRampUp, float fAtHalf, float fRampDown, float fAtZero )
{
	m_fEffectRampUp = fRampUp; 
	m_fEffectHoldAtHalf = fAtHalf; 
	m_fEffectRampDown = fRampDown;
	m_fEffectHoldAtZero = fAtZero;
	ASSERT( GetEffectPeriod() > 0 );
}

// effect "macros"

void Actor::SetEffectDiffuseBlink( float fEffectPeriodSeconds, RageColor c1, RageColor c2 )
{
	// todo: account for SSC_FUTURES -aj
	if( m_Effect != diffuse_blink )
	{
		m_Effect = diffuse_blink;
		m_fSecsIntoEffect = 0;
	}
	SetEffectPeriod( fEffectPeriodSeconds );
	m_effectColor1 = c1;
	m_effectColor2 = c2;
}

void Actor::SetEffectDiffuseShift( float fEffectPeriodSeconds, RageColor c1, RageColor c2 )
{
	// todo: account for SSC_FUTURES -aj
	if( m_Effect != diffuse_shift )
	{
		m_Effect = diffuse_shift;
		m_fSecsIntoEffect = 0;
	}
	SetEffectPeriod( fEffectPeriodSeconds );
	m_effectColor1 = c1;
	m_effectColor2 = c2;
}

void Actor::SetEffectDiffuseRamp( float fEffectPeriodSeconds, RageColor c1, RageColor c2 )
{
	// todo: account for SSC_FUTURES -aj
	if( m_Effect != diffuse_ramp )
	{
		m_Effect = diffuse_ramp;
		m_fSecsIntoEffect = 0;
	}
	SetEffectPeriod( fEffectPeriodSeconds );
	m_effectColor1 = c1;
	m_effectColor2 = c2;
}

void Actor::SetEffectGlowBlink( float fEffectPeriodSeconds, RageColor c1, RageColor c2 )
{
	// todo: account for SSC_FUTURES -aj
	if( m_Effect != glow_blink )
	{
		m_Effect = glow_blink;
		m_fSecsIntoEffect = 0;
	}
	SetEffectPeriod( fEffectPeriodSeconds );
	m_effectColor1 = c1;
	m_effectColor2 = c2;
}

void Actor::SetEffectGlowShift( float fEffectPeriodSeconds, RageColor c1, RageColor c2 )
{
	// todo: account for SSC_FUTURES -aj
	if( m_Effect != glow_shift )
	{
		m_Effect = glow_shift;
		m_fSecsIntoEffect = 0;
	}
	SetEffectPeriod( fEffectPeriodSeconds );
	m_effectColor1 = c1;
	m_effectColor2 = c2;
}

void Actor::SetEffectGlowRamp( float fEffectPeriodSeconds, RageColor c1, RageColor c2 )
{
	// todo: account for SSC_FUTURES -aj
	if( m_Effect != glow_ramp )
	{
		m_Effect = glow_ramp;
		m_fSecsIntoEffect = 0;
	}
	SetEffectPeriod( fEffectPeriodSeconds );
	m_effectColor1 = c1;
	m_effectColor2 = c2;
}

void Actor::SetEffectRainbow( float fEffectPeriodSeconds )
{
	// todo: account for SSC_FUTURES -aj
	if( m_Effect != rainbow )
	{
		m_Effect = rainbow;
		m_fSecsIntoEffect = 0;
	}
	SetEffectPeriod( fEffectPeriodSeconds );
}

void Actor::SetEffectWag( float fPeriod, RageVector3 vect )
{
	// todo: account for SSC_FUTURES -aj
	if( m_Effect != wag )
	{
		m_Effect = wag;
		m_fSecsIntoEffect = 0;
	}
	SetEffectPeriod( fPeriod );
	m_vEffectMagnitude = vect;
}

void Actor::SetEffectBounce( float fPeriod, RageVector3 vect )
{
	// todo: account for SSC_FUTURES -aj
	m_Effect = bounce;
	SetEffectPeriod( fPeriod );
	m_vEffectMagnitude = vect;
	m_fSecsIntoEffect = 0;
}

void Actor::SetEffectBob( float fPeriod, RageVector3 vect )
{
	// todo: account for SSC_FUTURES -aj
	if( m_Effect!=bob || GetEffectPeriod() != fPeriod )
	{
		m_Effect = bob;
		SetEffectPeriod( fPeriod );
		m_fSecsIntoEffect = 0;
	}
	m_vEffectMagnitude = vect;
}

void Actor::SetEffectSpin( RageVector3 vect )
{
	// todo: account for SSC_FUTURES -aj
	m_Effect = spin;
	m_vEffectMagnitude = vect;
}

void Actor::SetEffectVibrate( RageVector3 vect )
{
	// todo: account for SSC_FUTURES -aj
	m_Effect = vibrate;
	m_vEffectMagnitude = vect;
}

void Actor::SetEffectPulse( float fPeriod, float fMinZoom, float fMaxZoom )
{
	// todo: account for SSC_FUTURES -aj
	m_Effect = pulse;
	SetEffectPeriod( fPeriod );
	m_vEffectMagnitude[0] = fMinZoom;
	m_vEffectMagnitude[1] = fMaxZoom;
}


void Actor::AddRotationH( float rot )
{
	RageQuatMultiply( &DestTweenState().quat, DestTweenState().quat, RageQuatFromH(rot) );
}

void Actor::AddRotationP( float rot )
{
	RageQuatMultiply( &DestTweenState().quat, DestTweenState().quat, RageQuatFromP(rot) );
}

void Actor::AddRotationR( float rot )
{
	RageQuatMultiply( &DestTweenState().quat, DestTweenState().quat, RageQuatFromR(rot) );
}

void Actor::RunCommands( const LuaReference& cmds, const LuaReference *pParamTable )
{
	Lua *L = LUA->Get();

	// function
	cmds.PushSelf( L );
	ASSERT( !lua_isnil(L, -1) );

	// 1st parameter
	this->PushSelf( L );

	// 2nd parameter
	if( pParamTable == NULL )
		lua_pushnil( L );
	else
		pParamTable->PushSelf( L );

	// call function with 2 arguments and 0 results
	RString sError;
	if( !LuaHelpers::RunScriptOnStack(L, sError, 2, 0) )
		LOG->Warn( "Error playing command: %s", sError.c_str() );

	LUA->Release(L);
}

float Actor::GetTweenTimeLeft() const
{
	float tot = 0;

	tot += m_fHibernateSecondsLeft;

	for( unsigned i=0; i<m_Tweens.size(); ++i )
		tot += m_Tweens[i]->info.m_fTimeLeftInTween;

	return tot;
}

/* This is a hack to change all tween states while leaving existing tweens alone.
 *
 * Hmm. Most commands actually act on a TweenStateAndInfo, not the Actor itself.
 * Conceptually, it wouldn't be hard to give TweenState a presence in Lua, so
 * we can simply say eg. "for x in states(Actor) do x.SetDiffuseColor(c) end".
 * However, we'd then have to give every TweenState a userdata in Lua while it's
 * being manipulated, which would add overhead ... */
void Actor::SetGlobalDiffuseColor( RageColor c )
{
	for( int i=0; i<4; i++ ) // color, not alpha
	{
		for( unsigned ts = 0; ts < m_Tweens.size(); ++ts )
		{
			m_Tweens[ts]->state.diffuse[i].r = c.r; 
			m_Tweens[ts]->state.diffuse[i].g = c.g; 
			m_Tweens[ts]->state.diffuse[i].b = c.b; 
		}
		m_current.diffuse[i].r = c.r;
		m_current.diffuse[i].g = c.g;
		m_current.diffuse[i].b = c.b;
		m_start.diffuse[i].r = c.r;
		m_start.diffuse[i].g = c.g;
		m_start.diffuse[i].b = c.b;
	}
}

void Actor::SetDiffuseColor( RageColor c )
{
	for( int i=0; i<4; i++ )
	{
		DestTweenState().diffuse[i].r = c.r;
		DestTweenState().diffuse[i].g = c.g;
		DestTweenState().diffuse[i].b = c.b;
	}
}


void Actor::TweenState::Init()
{
	pos = RageVector3( 0, 0, 0 );
	rotation = RageVector3( 0, 0, 0 );
	quat = RageVector4( 0, 0, 0, 1 );
	scale = RageVector3( 1, 1, 1 );
	fSkewX = 0;
	fSkewY = 0;
	crop = RectF( 0,0,0,0 );
	fade = RectF( 0,0,0,0 );
	for( int i=0; i<4; i++ )
		diffuse[i] = RageColor( 1, 1, 1, 1 );
	glow = RageColor( 1, 1, 1, 0 );
	aux = 0;
}

bool Actor::TweenState::operator==( const TweenState &other ) const
{
#define COMPARE( x )	if( x != other.x ) return false;
	COMPARE( pos );
	COMPARE( rotation );
	COMPARE( quat );
	COMPARE( scale );
	COMPARE( fSkewX );
	COMPARE( fSkewY );
	COMPARE( crop );
	COMPARE( fade );
	for( unsigned i=0; i<ARRAYLEN(diffuse); i++ )
		COMPARE( diffuse[i] );
	COMPARE( glow );
	COMPARE( aux );
#undef COMPARE
	return true;
}

void Actor::TweenState::MakeWeightedAverage( TweenState& average_out, const TweenState& ts1, const TweenState& ts2, float fPercentBetween )
{
	average_out.pos		= lerp( fPercentBetween, ts1.pos,         ts2.pos );
	average_out.scale	= lerp( fPercentBetween, ts1.scale,       ts2.scale );
	average_out.rotation	= lerp( fPercentBetween, ts1.rotation,    ts2.rotation );
	RageQuatSlerp( &average_out.quat, ts1.quat, ts2.quat, fPercentBetween );
	average_out.fSkewX	= lerp( fPercentBetween, ts1.fSkewX,      ts2.fSkewX );
	average_out.fSkewY	= lerp( fPercentBetween, ts1.fSkewY,      ts2.fSkewY );

	average_out.crop.left	= lerp( fPercentBetween, ts1.crop.left,   ts2.crop.left	);
	average_out.crop.top	= lerp( fPercentBetween, ts1.crop.top,    ts2.crop.top );
	average_out.crop.right	= lerp( fPercentBetween, ts1.crop.right,  ts2.crop.right );
	average_out.crop.bottom	= lerp( fPercentBetween, ts1.crop.bottom, ts2.crop.bottom );

	average_out.fade.left	= lerp( fPercentBetween, ts1.fade.left,   ts2.fade.left );
	average_out.fade.top	= lerp( fPercentBetween, ts1.fade.top,    ts2.fade.top );
	average_out.fade.right	= lerp( fPercentBetween, ts1.fade.right,  ts2.fade.right );
	average_out.fade.bottom	= lerp( fPercentBetween, ts1.fade.bottom, ts2.fade.bottom );

	for( int i=0; i<4; ++i )
		average_out.diffuse[i] = lerp( fPercentBetween, ts1.diffuse[i], ts2.diffuse[i] );

	average_out.glow	= lerp( fPercentBetween, ts1.glow,        ts2.glow );
	average_out.aux		= lerp( fPercentBetween, ts1.aux,         ts2.aux );
}

void Actor::Sleep( float time )
{
	BeginTweening( time, TWEEN_LINEAR );
	BeginTweening( 0, TWEEN_LINEAR ); 
}

void Actor::QueueCommand( const RString& sCommandName )
{
	BeginTweening( 0, TWEEN_LINEAR );
	TweenInfo  &TI = m_Tweens.back()->info;
	TI.m_sCommandName = sCommandName;
}

void Actor::QueueMessage( const RString& sMessageName )
{
	// Hack: use "!" as a marker to broadcast a command, instead of playing a
	// command, so we don't have to add yet another element to every tween
	// state for this rarely-used command.
	BeginTweening( 0, TWEEN_LINEAR );
	TweenInfo &TI = m_Tweens.back()->info;
	TI.m_sCommandName = "!" + sMessageName;
}

void Actor::AddCommand( const RString &sCmdName, apActorCommands apac )
{
	if( HasCommand(sCmdName) )
	{
		RString sWarning = GetLineage()+"'s command '"+sCmdName+"' defined twice";
		Dialog::OK( sWarning, "COMMAND_DEFINED_TWICE" );
	}

	RString sMessage;
	if( GetMessageNameFromCommandName(sCmdName, sMessage) )
	{
		SubscribeToMessage( sMessage );
		m_mapNameToCommands[sMessage] = apac;	// sCmdName w/o "Message" at the end
	}
	else
	{
		m_mapNameToCommands[sCmdName] = apac;
	}
}

bool Actor::HasCommand( const RString &sCmdName ) const
{
	return GetCommand(sCmdName) != NULL;
}

const apActorCommands *Actor::GetCommand( const RString &sCommandName ) const
{
	map<RString, apActorCommands>::const_iterator it = m_mapNameToCommands.find( sCommandName );
	if( it == m_mapNameToCommands.end() )
		return NULL;
	return &it->second;
}

void Actor::HandleMessage( const Message &msg )
{
	PlayCommandNoRecurse( msg );
}

void Actor::PlayCommandNoRecurse( const Message &msg )
{
	const apActorCommands *pCmd = GetCommand( msg.GetName() );
	if( pCmd != NULL )
		RunCommands( *pCmd, &msg.GetParamTable() );
}

void Actor::PushContext( lua_State *L )
{
	// self.ctx should already exist
	m_pLuaInstance->PushSelf( L );
	lua_getfield( L, -1, "ctx" );
	lua_remove( L, -2 );
}

void Actor::SetParent( Actor *pParent )
{
	m_pParent = pParent;

	Lua *L = LUA->Get();
		int iTop = lua_gettop( L );

		this->PushContext( L );
		lua_pushstring( L, "__index" );
		pParent->PushContext( L );
		lua_settable( L, -3 );

		lua_settop( L, iTop );
	LUA->Release( L );
}

Actor::TweenInfo::TweenInfo()
{
	m_pTween = NULL;
}

Actor::TweenInfo::~TweenInfo()
{
	delete m_pTween;
}

Actor::TweenInfo::TweenInfo( const TweenInfo &cpy )
{
	m_pTween = NULL;
	*this = cpy;
}

Actor::TweenInfo &Actor::TweenInfo::operator=( const TweenInfo &rhs )
{
	delete m_pTween;
	m_pTween = (rhs.m_pTween? rhs.m_pTween->Copy():NULL);
	m_fTimeLeftInTween = rhs.m_fTimeLeftInTween;
	m_fTweenTime = rhs.m_fTweenTime;
	m_sCommandName = rhs.m_sCommandName;
	return *this;
}

// lua start
#include "LuaBinding.h"

/** @brief Allow Lua to have access to the Actor. */ 
class LunaActor : public Luna<Actor>
{
public:
	static int name( T* p, lua_State *L )			{ p->SetName(SArg(1)); return 0; }
	static int sleep( T* p, lua_State *L )			{ p->Sleep(FArg(1)); return 0; }
	static int linear( T* p, lua_State *L )			{ p->BeginTweening(FArg(1),TWEEN_LINEAR); return 0; }
	static int accelerate( T* p, lua_State *L )		{ p->BeginTweening(FArg(1),TWEEN_ACCELERATE); return 0; }
	static int decelerate( T* p, lua_State *L )		{ p->BeginTweening(FArg(1),TWEEN_DECELERATE); return 0; }
	static int spring( T* p, lua_State *L )			{ p->BeginTweening(FArg(1),TWEEN_SPRING); return 0; }
	static int tween( T* p, lua_State *L )
	{
		ITween *pTween = ITween::CreateFromStack( L, 2 );
		p->BeginTweening( FArg(1), pTween );
		return 0;
	}
	static int stoptweening( T* p, lua_State * )		{ p->StopTweening(); return 0; }
	static int finishtweening( T* p, lua_State * )		{ p->FinishTweening(); return 0; }
	static int hurrytweening( T* p, lua_State *L )		{ p->HurryTweening(FArg(1)); return 0; }
	static int GetTweenTimeLeft( T* p, lua_State *L )	{ lua_pushnumber( L, p->GetTweenTimeLeft() ); return 1; }
	static int x( T* p, lua_State *L )			{ p->SetX(FArg(1)); return 0; }
	static int y( T* p, lua_State *L )			{ p->SetY(FArg(1)); return 0; }
	static int z( T* p, lua_State *L )			{ p->SetZ(FArg(1)); return 0; }
	static int xy( T* p, lua_State *L )			{ p->SetXY(FArg(1),FArg(2)); return 0; }
	static int addx( T* p, lua_State *L )			{ p->AddX(FArg(1)); return 0; }
	static int addy( T* p, lua_State *L )			{ p->AddY(FArg(1)); return 0; }
	static int addz( T* p, lua_State *L )			{ p->AddZ(FArg(1)); return 0; }
	static int zoom( T* p, lua_State *L )			{ p->SetZoom(FArg(1)); return 0; }
	static int zoomx( T* p, lua_State *L )			{ p->SetZoomX(FArg(1)); return 0; }
	static int zoomy( T* p, lua_State *L )			{ p->SetZoomY(FArg(1)); return 0; }
	static int zoomz( T* p, lua_State *L )			{ p->SetZoomZ(FArg(1)); return 0; }
	static int zoomto( T* p, lua_State *L )			{ p->ZoomTo(FArg(1), FArg(2)); return 0; }
	static int zoomtowidth( T* p, lua_State *L )		{ p->ZoomToWidth(FArg(1)); return 0; }
	static int zoomtoheight( T* p, lua_State *L )		{ p->ZoomToHeight(FArg(1)); return 0; }
	static int setsize( T* p, lua_State *L )		{ p->SetWidth(FArg(1)); p->SetHeight(FArg(2)); return 0; }
	static int SetWidth( T* p, lua_State *L )		{ p->SetWidth(FArg(1)); return 0; }
	static int SetHeight( T* p, lua_State *L )		{ p->SetHeight(FArg(1)); return 0; }
	static int basealpha( T* p, lua_State *L )		{ p->SetBaseAlpha(FArg(1)); return 0; }
	static int basezoom( T* p, lua_State *L )		{ p->SetBaseZoom(FArg(1)); return 0; }
	static int basezoomx( T* p, lua_State *L )		{ p->SetBaseZoomX(FArg(1)); return 0; }
	static int basezoomy( T* p, lua_State *L )		{ p->SetBaseZoomY(FArg(1)); return 0; }
	static int basezoomz( T* p, lua_State *L )		{ p->SetBaseZoomZ(FArg(1)); return 0; }
	static int stretchto( T* p, lua_State *L )		{ p->StretchTo( RectF(FArg(1),FArg(2),FArg(3),FArg(4)) ); return 0; }
	static int cropleft( T* p, lua_State *L )		{ p->SetCropLeft(FArg(1)); return 0; }
	static int croptop( T* p, lua_State *L )		{ p->SetCropTop(FArg(1)); return 0; }
	static int cropright( T* p, lua_State *L )		{ p->SetCropRight(FArg(1)); return 0; }
	static int cropbottom( T* p, lua_State *L )		{ p->SetCropBottom(FArg(1)); return 0; }
	static int fadeleft( T* p, lua_State *L )		{ p->SetFadeLeft(FArg(1)); return 0; }
	static int fadetop( T* p, lua_State *L )		{ p->SetFadeTop(FArg(1)); return 0; }
	static int faderight( T* p, lua_State *L )		{ p->SetFadeRight(FArg(1)); return 0; }
	static int fadebottom( T* p, lua_State *L )		{ p->SetFadeBottom(FArg(1)); return 0; }
	static int diffuse( T* p, lua_State *L )		{ RageColor c; c.FromStackCompat( L, 1 ); p->SetDiffuse( c ); return 0; }
	static int diffuseupperleft( T* p, lua_State *L )	{ RageColor c; c.FromStackCompat( L, 1 ); p->SetDiffuseUpperLeft( c ); return 0; }
	static int diffuseupperright( T* p, lua_State *L )	{ RageColor c; c.FromStackCompat( L, 1 ); p->SetDiffuseUpperRight( c ); return 0; }
	static int diffuselowerleft( T* p, lua_State *L )	{ RageColor c; c.FromStackCompat( L, 1 ); p->SetDiffuseLowerLeft( c ); return 0; }
	static int diffuselowerright( T* p, lua_State *L )	{ RageColor c; c.FromStackCompat( L, 1 ); p->SetDiffuseLowerRight( c ); return 0; }
	static int diffuseleftedge( T* p, lua_State *L )	{ RageColor c; c.FromStackCompat( L, 1 ); p->SetDiffuseLeftEdge( c ); return 0; }
	static int diffuserightedge( T* p, lua_State *L )	{ RageColor c; c.FromStackCompat( L, 1 ); p->SetDiffuseRightEdge( c ); return 0; }
	static int diffusetopedge( T* p, lua_State *L )		{ RageColor c; c.FromStackCompat( L, 1 ); p->SetDiffuseTopEdge( c ); return 0; }
	static int diffusebottomedge( T* p, lua_State *L )	{ RageColor c; c.FromStackCompat( L, 1 ); p->SetDiffuseBottomEdge( c ); return 0; }
	static int diffusealpha( T* p, lua_State *L )		{ p->SetDiffuseAlpha(FArg(1)); return 0; }
	static int diffusecolor( T* p, lua_State *L )		{ RageColor c; c.FromStackCompat( L, 1 ); p->SetDiffuseColor( c ); return 0; }
	static int glow( T* p, lua_State *L )			{ RageColor c; c.FromStackCompat( L, 1 ); p->SetGlow( c ); return 0; }
	static int aux( T* p, lua_State *L )			{ p->SetAux( FArg(1) ); return 0; }
	static int getaux( T* p, lua_State *L )			{ lua_pushnumber( L, p->GetAux() ); return 1; }
	static int rotationx( T* p, lua_State *L )		{ p->SetRotationX(FArg(1)); return 0; }
	static int rotationy( T* p, lua_State *L )		{ p->SetRotationY(FArg(1)); return 0; }
	static int rotationz( T* p, lua_State *L )		{ p->SetRotationZ(FArg(1)); return 0; }
	static int addrotationx( T* p, lua_State *L )		{ p->AddRotationX(FArg(1)); return 0; }
	static int addrotationy( T* p, lua_State *L )		{ p->AddRotationY(FArg(1)); return 0; }
	static int addrotationz( T* p, lua_State *L )		{ p->AddRotationZ(FArg(1)); return 0; }
	static int getrotation( T* p, lua_State *L )		{ lua_pushnumber(L, p->GetRotationX()); lua_pushnumber(L, p->GetRotationY()); lua_pushnumber(L, p->GetRotationZ()); return 3; }
	static int baserotationx( T* p, lua_State *L )		{ p->SetBaseRotationX(FArg(1)); return 0; }
	static int baserotationy( T* p, lua_State *L )		{ p->SetBaseRotationY(FArg(1)); return 0; }
	static int baserotationz( T* p, lua_State *L )		{ p->SetBaseRotationZ(FArg(1)); return 0; }
	static int skewx( T* p, lua_State *L )			{ p->SetSkewX(FArg(1)); return 0; }
	static int skewy( T* p, lua_State *L )			{ p->SetSkewY(FArg(1)); return 0; }
	static int heading( T* p, lua_State *L )		{ p->AddRotationH(FArg(1)); return 0; }
	static int pitch( T* p, lua_State *L )			{ p->AddRotationP(FArg(1)); return 0; }
	static int roll( T* p, lua_State *L )			{ p->AddRotationR(FArg(1)); return 0; }
	static int shadowlength( T* p, lua_State *L )		{ p->SetShadowLength(FArg(1)); return 0; }
	static int shadowlengthx( T* p, lua_State *L )		{ p->SetShadowLengthX(FArg(1)); return 0; }
	static int shadowlengthy( T* p, lua_State *L )		{ p->SetShadowLengthY(FArg(1)); return 0; }
	static int shadowcolor( T* p, lua_State *L )		{ RageColor c; c.FromStackCompat( L, 1 ); p->SetShadowColor( c ); return 0; }
	static int horizalign( T* p, lua_State *L )		{ p->SetHorizAlign(Enum::Check<HorizAlign>(L, 1)); return 0; }
	static int vertalign( T* p, lua_State *L )		{ p->SetVertAlign(Enum::Check<VertAlign>(L, 1)); return 0; }
	static int halign( T* p, lua_State *L )			{ p->SetHorizAlign(FArg(1)); return 0; }
	static int valign( T* p, lua_State *L )			{ p->SetVertAlign(FArg(1)); return 0; }
	static int diffuseblink( T* p, lua_State * )		{ p->SetEffectDiffuseBlink(); return 0; }
	static int diffuseshift( T* p, lua_State * )		{ p->SetEffectDiffuseShift(); return 0; }
	static int diffuseramp( T* p, lua_State * )		{ p->SetEffectDiffuseRamp(); return 0; }
	static int glowblink( T* p, lua_State * )		{ p->SetEffectGlowBlink(); return 0; }
	static int glowshift( T* p, lua_State * )		{ p->SetEffectGlowShift(); return 0; }
	static int glowramp( T* p, lua_State * )		{ p->SetEffectGlowRamp(); return 0; }
	static int rainbow( T* p, lua_State * )		{ p->SetEffectRainbow(); return 0; }
	static int wag( T* p, lua_State * )			{ p->SetEffectWag(); return 0; }
	static int bounce( T* p, lua_State * )			{ p->SetEffectBounce(); return 0; }
	static int bob( T* p, lua_State * )			{ p->SetEffectBob(); return 0; }
	static int pulse( T* p, lua_State * )			{ p->SetEffectPulse(); return 0; }
	static int spin( T* p, lua_State * )			{ p->SetEffectSpin(); return 0; }
	static int vibrate( T* p, lua_State * )		{ p->SetEffectVibrate(); return 0; }
	static int stopeffect( T* p, lua_State * )		{ p->StopEffect(); return 0; }
	static int effectcolor1( T* p, lua_State *L )		{ RageColor c; c.FromStackCompat( L, 1 ); p->SetEffectColor1( c ); return 0; }
	static int effectcolor2( T* p, lua_State *L )		{ RageColor c; c.FromStackCompat( L, 1 ); p->SetEffectColor2( c ); return 0; }
	static int effectperiod( T* p, lua_State *L )		{ p->SetEffectPeriod(FArg(1)); return 0; }
	static int effecttiming( T* p, lua_State *L )		{ p->SetEffectTiming(FArg(1),FArg(2),FArg(3),FArg(4)); return 0; }
	static int effectoffset( T* p, lua_State *L )		{ p->SetEffectOffset(FArg(1)); return 0; }
	static int effectclock( T* p, lua_State *L )		{ p->SetEffectClockString(SArg(1)); return 0; }
	static int effectmagnitude( T* p, lua_State *L )	{ p->SetEffectMagnitude( RageVector3(FArg(1),FArg(2),FArg(3)) ); return 0; }
	static int geteffectmagnitude( T* p, lua_State *L )	{ RageVector3 v = p->GetEffectMagnitude(); lua_pushnumber(L, v[0]); lua_pushnumber(L, v[1]); lua_pushnumber(L, v[2]); return 3; }
	static int scaletocover( T* p, lua_State *L )		{ p->ScaleToCover( RectF(FArg(1), FArg(2), FArg(3), FArg(4)) ); return 0; }
	static int scaletofit( T* p, lua_State *L )		{ p->ScaleToFitInside( RectF(FArg(1), FArg(2), FArg(3), FArg(4)) ); return 0; }
	static int animate( T* p, lua_State *L )		{ p->EnableAnimation(BIArg(1)); return 0; }
	static int play( T* p, lua_State * )			{ p->EnableAnimation(true); return 0; }
	static int pause( T* p, lua_State * )			{ p->EnableAnimation(false); return 0; }
	static int setstate( T* p, lua_State *L )		{ p->SetState(IArg(1)); return 0; }
	static int GetNumStates( T* p, lua_State *L )		{ LuaHelpers::Push( L, p->GetNumStates() ); return 1; }
	static int texturewrapping( T* p, lua_State *L )	{ p->SetTextureWrapping(BIArg(1)); return 0; }
	static int SetTextureFiltering( T* p, lua_State *L )	{ p->SetTextureFiltering(BArg(1)); return 0; }
	static int blend( T* p, lua_State *L )			{ p->SetBlendMode( Enum::Check<BlendMode>(L, 1) ); return 0; }
	static int zbuffer( T* p, lua_State *L )		{ p->SetUseZBuffer(BIArg(1)); return 0; }
	static int ztest( T* p, lua_State *L )			{ p->SetZTestMode((BIArg(1))?ZTEST_WRITE_ON_PASS:ZTEST_OFF); return 0; }
	static int ztestmode( T* p, lua_State *L )		{ p->SetZTestMode( Enum::Check<ZTestMode>(L, 1) ); return 0; }
	static int zwrite( T* p, lua_State *L )			{ p->SetZWrite(BIArg(1)); return 0; }
	static int zbias( T* p, lua_State *L )			{ p->SetZBias(FArg(1)); return 0; }
	static int clearzbuffer( T* p, lua_State *L )		{ p->SetClearZBuffer(BIArg(1)); return 0; }
	static int backfacecull( T* p, lua_State *L )		{ p->SetCullMode((BIArg(1)) ? CULL_BACK : CULL_NONE); return 0; }
	static int cullmode( T* p, lua_State *L )		{ p->SetCullMode( Enum::Check<CullMode>(L, 1)); return 0; }
	static int visible( T* p, lua_State *L )		{ p->SetVisible(BIArg(1)); return 0; }
	static int hibernate( T* p, lua_State *L )		{ p->SetHibernate(FArg(1)); return 0; }
	static int draworder( T* p, lua_State *L )		{ p->SetDrawOrder(IArg(1)); return 0; }
	static int playcommand( T* p, lua_State *L )
	{
		if( !lua_istable(L, 2) && !lua_isnoneornil(L, 2) )
			luaL_typerror( L, 2, "table or nil" );

		LuaReference ParamTable;
		lua_pushvalue( L, 2 );
		ParamTable.SetFromStack( L );

		Message msg( SArg(1), ParamTable );
		p->HandleMessage( msg );

		return 0;
	}
	static int queuecommand( T* p, lua_State *L )		{ p->QueueCommand(SArg(1)); return 0; }
	static int queuemessage( T* p, lua_State *L )		{ p->QueueMessage(SArg(1)); return 0; }
	static int addcommand( T* p, lua_State *L )
	{
		LuaReference *pRef = new LuaReference;
		pRef->SetFromStack( L );
		p->AddCommand( SArg(1), apActorCommands(pRef) );
		return 0;
	}
	static int GetCommand( T* p, lua_State *L )
	{
		const apActorCommands *pCommand = p->GetCommand(SArg(1));
		if( pCommand == NULL )
			lua_pushnil( L );
		else
			(*pCommand)->PushSelf(L);

		return 1;
	}
	static int RunCommandsRecursively( T* p, lua_State *L )
	{
		luaL_checktype( L, 1, LUA_TFUNCTION );
		if( !lua_istable(L, 2) && !lua_isnoneornil(L, 2) )
			luaL_typerror( L, 2, "table or nil" );

		LuaReference ref;
		lua_pushvalue( L, 1 );
		ref.SetFromStack( L );

		LuaReference ParamTable;
		lua_pushvalue( L, 2 );
		ParamTable.SetFromStack( L );

		p->RunCommandsRecursively( ref, &ParamTable );
		return 0;
	}

	static int GetX( T* p, lua_State *L )			{ lua_pushnumber( L, p->GetX() ); return 1; }
	static int GetY( T* p, lua_State *L )			{ lua_pushnumber( L, p->GetY() ); return 1; }
	static int GetZ( T* p, lua_State *L )			{ lua_pushnumber( L, p->GetZ() ); return 1; }
	static int GetWidth( T* p, lua_State *L )		{ lua_pushnumber( L, p->GetUnzoomedWidth() ); return 1; }
	static int GetHeight( T* p, lua_State *L )		{ lua_pushnumber( L, p->GetUnzoomedHeight() ); return 1; }
	static int GetZoomedWidth( T* p, lua_State *L )		{ lua_pushnumber( L, p->GetZoomedWidth() ); return 1; }
	static int GetZoomedHeight( T* p, lua_State *L )	{ lua_pushnumber( L, p->GetZoomedHeight() ); return 1; }
	static int GetZoom( T* p, lua_State *L )		{ lua_pushnumber( L, p->GetZoom() ); return 1; }
	static int GetZoomX( T* p, lua_State *L )		{ lua_pushnumber( L, p->GetZoomX() ); return 1; }
	static int GetZoomY( T* p, lua_State *L )		{ lua_pushnumber( L, p->GetZoomY() ); return 1; }
	static int GetZoomZ( T* p, lua_State *L )		{ lua_pushnumber( L, p->GetZoomZ() ); return 1; }
	static int GetBaseZoomX( T* p, lua_State *L )		{ lua_pushnumber( L, p->GetBaseZoomX() ); return 1; }
	static int GetBaseZoomY( T* p, lua_State *L )		{ lua_pushnumber( L, p->GetBaseZoomY() ); return 1; }
	static int GetBaseZoomZ( T* p, lua_State *L )		{ lua_pushnumber( L, p->GetBaseZoomZ() ); return 1; }
	static int GetRotationX( T* p, lua_State *L )		{ lua_pushnumber( L, p->GetRotationX() ); return 1; }
	static int GetRotationY( T* p, lua_State *L )		{ lua_pushnumber( L, p->GetRotationY() ); return 1; }
	static int GetRotationZ( T* p, lua_State *L )		{ lua_pushnumber( L, p->GetRotationZ() ); return 1; }
	static int GetSecsIntoEffect( T* p, lua_State *L )	{ lua_pushnumber( L, p->GetSecsIntoEffect() ); return 1; }
	static int GetEffectDelta( T* p, lua_State *L )		{ lua_pushnumber( L, p->GetEffectDelta() ); return 1; }
	DEFINE_METHOD( GetDiffuse, GetDiffuse() )
	DEFINE_METHOD( GetGlow, GetGlow() )
	static int GetDiffuseAlpha( T* p, lua_State *L )	{ lua_pushnumber( L, p->GetDiffuseAlpha() ); return 1; }
	static int GetVisible( T* p, lua_State *L )		{ lua_pushboolean( L, p->GetVisible() ); return 1; }
	static int GetHAlign( T* p, lua_State *L )	{ lua_pushnumber( L, p->GetHorizAlign() ); return 1; }
	static int GetVAlign( T* p, lua_State *L )	{ lua_pushnumber( L, p->GetVertAlign() ); return 1; }

	static int GetName( T* p, lua_State *L )		{ lua_pushstring( L, p->GetName() ); return 1; }
	static int GetParent( T* p, lua_State *L )
	{
		Actor *pParent = p->GetParent();
		if( pParent == NULL )
			lua_pushnil( L );
		else
			pParent->PushSelf(L);
		return 1;
	}
	static int Draw( T* p, lua_State *L )
	{
		LUA->YieldLua();
		p->Draw();
		LUA->UnyieldLua();
		return 0;
	}

	LunaActor()
	{
  		ADD_METHOD( name );
  		ADD_METHOD( sleep );
		ADD_METHOD( linear );
		ADD_METHOD( accelerate );
		ADD_METHOD( decelerate );
		ADD_METHOD( spring );
		ADD_METHOD( tween );
		ADD_METHOD( stoptweening );
		ADD_METHOD( finishtweening );
		ADD_METHOD( hurrytweening );
		ADD_METHOD( GetTweenTimeLeft );
		ADD_METHOD( x );
		ADD_METHOD( y );
		ADD_METHOD( z );
		ADD_METHOD( xy );
		ADD_METHOD( addx );
		ADD_METHOD( addy );
		ADD_METHOD( addz );
		ADD_METHOD( zoom );
		ADD_METHOD( zoomx );
		ADD_METHOD( zoomy );
		ADD_METHOD( zoomz );
		ADD_METHOD( zoomto );
		ADD_METHOD( zoomtowidth );
		ADD_METHOD( zoomtoheight );
		ADD_METHOD( setsize );
		ADD_METHOD( SetWidth );
		ADD_METHOD( SetHeight );
		ADD_METHOD( basealpha );
		ADD_METHOD( basezoom );
		ADD_METHOD( basezoomx );
		ADD_METHOD( basezoomy );
		ADD_METHOD( basezoomz );
		ADD_METHOD( stretchto );
		ADD_METHOD( cropleft );
		ADD_METHOD( croptop );
		ADD_METHOD( cropright );
		ADD_METHOD( cropbottom );
		ADD_METHOD( fadeleft );
		ADD_METHOD( fadetop );
		ADD_METHOD( faderight );
		ADD_METHOD( fadebottom );
		ADD_METHOD( diffuse );
		ADD_METHOD( diffuseupperleft );
		ADD_METHOD( diffuseupperright );
		ADD_METHOD( diffuselowerleft );
		ADD_METHOD( diffuselowerright );
		ADD_METHOD( diffuseleftedge );
		ADD_METHOD( diffuserightedge );
		ADD_METHOD( diffusetopedge );
		ADD_METHOD( diffusebottomedge );
		ADD_METHOD( diffusealpha );
		ADD_METHOD( diffusecolor );
		ADD_METHOD( glow );
		ADD_METHOD( aux );
		ADD_METHOD( getaux );
		ADD_METHOD( rotationx );
		ADD_METHOD( rotationy );
		ADD_METHOD( rotationz );
		ADD_METHOD( addrotationx );
		ADD_METHOD( addrotationy );
		ADD_METHOD( addrotationz );
		ADD_METHOD( getrotation );
		ADD_METHOD( baserotationx );
		ADD_METHOD( baserotationy );
		ADD_METHOD( baserotationz );
		ADD_METHOD( skewx );
		ADD_METHOD( skewy );
		ADD_METHOD( heading );
		ADD_METHOD( pitch );
		ADD_METHOD( roll );
		ADD_METHOD( shadowlength );
		ADD_METHOD( shadowlengthx );
		ADD_METHOD( shadowlengthy );
		ADD_METHOD( shadowcolor );
		ADD_METHOD( horizalign );
		ADD_METHOD( vertalign );
		ADD_METHOD( halign );
		ADD_METHOD( valign );
		ADD_METHOD( diffuseblink );
		ADD_METHOD( diffuseshift );
		ADD_METHOD( diffuseramp );
		ADD_METHOD( glowblink );
		ADD_METHOD( glowshift );
		ADD_METHOD( glowramp );
		ADD_METHOD( rainbow );
		ADD_METHOD( wag );
		ADD_METHOD( bounce );
		ADD_METHOD( bob );
		ADD_METHOD( pulse );
		ADD_METHOD( spin );
		ADD_METHOD( vibrate );
		ADD_METHOD( stopeffect );
		ADD_METHOD( effectcolor1 );
		ADD_METHOD( effectcolor2 );
		ADD_METHOD( effectperiod );
		ADD_METHOD( effecttiming );
		ADD_METHOD( effectoffset );
		ADD_METHOD( effectclock );
		ADD_METHOD( effectmagnitude );
		ADD_METHOD( geteffectmagnitude );
		ADD_METHOD( scaletocover );
		ADD_METHOD( scaletofit );
		ADD_METHOD( animate );
		ADD_METHOD( play );
		ADD_METHOD( pause );
		ADD_METHOD( setstate );
		ADD_METHOD( GetNumStates );
		ADD_METHOD( texturewrapping );
		ADD_METHOD( SetTextureFiltering );
		ADD_METHOD( blend );
		ADD_METHOD( zbuffer );
		ADD_METHOD( ztest );
		ADD_METHOD( ztestmode );
		ADD_METHOD( zwrite );
		ADD_METHOD( zbias );
		ADD_METHOD( clearzbuffer );
		ADD_METHOD( backfacecull );
		ADD_METHOD( cullmode );
		ADD_METHOD( visible );
		ADD_METHOD( hibernate );
		ADD_METHOD( draworder );
		ADD_METHOD( playcommand );
		ADD_METHOD( queuecommand );
		ADD_METHOD( queuemessage );
		ADD_METHOD( addcommand );
		ADD_METHOD( GetCommand );
		ADD_METHOD( RunCommandsRecursively );

		ADD_METHOD( GetX );
		ADD_METHOD( GetY );
		ADD_METHOD( GetZ );
		ADD_METHOD( GetWidth );
		ADD_METHOD( GetHeight );
		ADD_METHOD( GetZoomedWidth );
		ADD_METHOD( GetZoomedHeight );
		ADD_METHOD( GetZoom );
		ADD_METHOD( GetZoomX );
		ADD_METHOD( GetZoomY );
		ADD_METHOD( GetZoomZ );
		ADD_METHOD( GetRotationX );
		ADD_METHOD( GetRotationY );
		ADD_METHOD( GetRotationZ );
		ADD_METHOD( GetBaseZoomX );
		ADD_METHOD( GetBaseZoomY );
		ADD_METHOD( GetBaseZoomZ );
		ADD_METHOD( GetSecsIntoEffect );
		ADD_METHOD( GetEffectDelta );
		ADD_METHOD( GetDiffuse );
		ADD_METHOD( GetDiffuseAlpha );
		ADD_METHOD( GetGlow );
		ADD_METHOD( GetVisible );
		ADD_METHOD( GetHAlign );
		ADD_METHOD( GetVAlign );

		ADD_METHOD( GetName );
		ADD_METHOD( GetParent );

		ADD_METHOD( Draw );
	}
};

LUA_REGISTER_INSTANCED_BASE_CLASS( Actor )
// lua end


/*
 * (c) 2001-2004 Chris Danford
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, and/or sell copies of the Software, and to permit persons to
 * whom the Software is furnished to do so, provided that the above
 * copyright notice(s) and this permission notice appear in all copies of
 * the Software and that both the above copyright notice(s) and this
 * permission notice appear in supporting documentation.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF
 * THIRD PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS
 * INCLUDED IN THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
