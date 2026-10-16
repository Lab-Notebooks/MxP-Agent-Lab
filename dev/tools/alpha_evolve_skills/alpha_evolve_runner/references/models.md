<!-- Source: https://g3doc.corp.google.com/cloud/ai/science/agents/alpha_evolve/skills/alpha_evolve_runner/references/models.md?cl=head -->

# Generation Models 

- Edited 2026-06-30 
On this page 

# Generation Models 

--models passes model names straight through to the AlphaEvolve API, which
validates them. Use the API model name (e.g. gemini-3.5-flash ), not the
old preset enum. The legacy singular model flag is deprecated — use --models . 

## Syntax 

--models is repeatable (one flag per model). Each value is either a bare name
or a comma-separated key=value spec: 

# Single model (weight defaults to 1.0) --models gemini- 3.5 -flash # Weighted mixture (relative weights) --models name=gemini- 3.5 -flash,weight=0.9 \
  --models name=gemini- 3.1 -pro-preview,weight=0.1 

Same syntax on ae config --models=... and ae experiment create --models ... . 

## Recommended models 

Model name Regions Notes 
gemini-3.5-flash global, us, eu Recommended default 
gemini-3.1-pro-preview global Higher quality; mix with flash 

## Legacy models (also supported) 

Older models still in the broader allowlist; they work but are not part of the
GA set above: 

Model name Regions Notes 
gemini-2.5-flash global, us, eu Legacy Flash 
gemini-2.5-pro global, us, eu Legacy Pro 
gemini-3-flash-preview global Preview Flash 

## Notes 

- weight is optional and relative (ratios, not sum-to-1); omit it for a
single model — the API defaults a missing weight to 1.0. 
- At most two models may be combined in one experiment. 
- Model names and weights are validated by the API (it returns INVALID_ARGUMENT for an unknown name or unavailable region). The CLI keeps
no model allowlist of its own, so it never goes stale.
