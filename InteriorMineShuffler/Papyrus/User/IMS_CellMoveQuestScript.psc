ScriptName IMS_CellMoveQuestScript Extends ReferenceAlias

Actor PlayerRef

Event OnAliasInit()
	RegisterForPlayerTeleport()
	PlayerRef = GetActorRef()
EndEvent

Event OnPlayerTeleport()
	InteriorMineShuffler.shuffleMine()
EndEvent