target_sources_ifdef(CONFIG_ALTRUIST_IDENTITY app PRIVATE
  identity.c
  monocypher/monocypher.c
  monocypher/monocypher-ed25519.c
)
