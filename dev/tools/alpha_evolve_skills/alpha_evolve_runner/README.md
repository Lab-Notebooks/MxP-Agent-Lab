<!-- Source: https://g3doc.corp.google.com/cloud/ai/science/agents/alpha_evolve/skills/alpha_evolve_runner/README.md?cl=head -->

# Alpha Evolve Experiment Runner Skill 

- Edited 2026-07-14 
On this page 

# Alpha Evolve Experiment Runner Skill 

AI agent skill for configuring, verifying, and launching AlphaEvolve
experiments using the ae CLI. 

## Overview 

Takes experiment artifacts (program, evaluator, problem description) and
launches an experiment on the AlphaEvolve backend through the ae CLI. 

This is one of three AlphaEvolve skills: 

- Experiment Design -- Problem definition, initial program, evaluator 
- Experiment Runner (this skill) -- Configuration, verification, launch 
- Experiment Monitor -- Evaluation loop, progress tracking, results 
Handles: 

- GCP configuration auto-discovery and setup 
- API connectivity verification 
- Evaluator validation with baseline scoring 
- Experiment creation and launch 

## Handoff 

- Input: Artifacts from the Design skill or user-provided files 
- Output: An experiment nickname (e.g., brave-otter ) for the Monitor
skill 

## Prerequisites 

- ae CLI installed (see the ae CLI documentation for installation instructions) 
- gcloud CLI installed (the ae CLI discovers it automatically; run gcloud auth application-default login if credentials are needed) 
- A Google Cloud project with AlphaEvolve enabled 

## Skill Structure 

alpha_evolve_runner/
  README.md
  SKILL.md # Main skill instructions
  references/
    cli_reference.md # ae CLI command reference
    debugging.md # Troubleshooting guide
    models.md # Generation model reference
