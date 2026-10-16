<!-- Source: https://g3doc.corp.google.com/cloud/ai/science/agents/alpha_evolve/skills/alpha_evolve_monitor/README.md?cl=head -->

# Alpha Evolve Experiment Monitor Skill 

- Edited 2026-07-14 
On this page 

# Alpha Evolve Experiment Monitor Skill 

AI agent skill for monitoring running AlphaEvolve experiments, managing the
evaluation control loop, and reporting results. 

## Overview 

Takes an experiment identifier (nickname, ID, or resource name) for an
already-running AlphaEvolve experiment and provides two core functions: 

- Control loop -- Acquires candidate programs, evaluates them locally, and
submits scores back to the backend using ae experiment run . 
- Monitoring loop -- Polls experiment status and visualizes progress using ae experiment describe . 
The skill periodically checks for updates and presents a live experiment
report to the user with current state, evaluation count, best score, and
top programs. 

This is one of three AlphaEvolve skills: 

- Experiment Design -- Problem definition, initial program, evaluator 
- Experiment Runner -- Configuration, verification, launch 
- Experiment Monitor (this skill) -- Evaluation loop, progress tracking,
results 

## Handoff 

- Input: An experiment nickname (e.g., brave-otter ) and evaluator file
path from the Runner skill or the user 
- Output: Periodic experiment reports with status, scores, and top
programs 

## Prerequisites 

- ae CLI installed (see the ae CLI documentation for installation instructions) 
- An already-running AlphaEvolve experiment (created by the Runner skill or
manually via ae experiment create + ae experiment start ) 

## Skill Structure 

alpha_evolve_monitor/
  README.md
  SKILL.md # Main skill instructions
  references/
    cli_reference.md # ae CLI command reference for monitoring
    debugging.md # Troubleshooting guide
    models.md # Generation model reference
