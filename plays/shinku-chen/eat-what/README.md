<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# What to Eat Today

A button-driven food roulette that turns the AI Passport into a "what should I
eat?" spinner. Hold a key and watch a recommendation cycle through your options;
let go to land on the one you want.

## Publish information

- **Title**: What to Eat Today
- **Description**: Turn your AI Passport into a "what should I eat?" spinner.
  Hold one button to cycle the lunch prompts, hold the other to spin through all
  kinds of food, and let go to land on the one you want. Perfect for the daily
  "what's for lunch" dilemma, or passing it around at a gathering for everyone to
  pick in turn.
## What it does

- **Boots straight into the app**: the firmware opens the roulette directly with
  no main menu.
- **Lunch-prompt animation** (hold the first button): loops a "what's for lunch?"
  prompt at 10 fps.
- **Food-picker animation** (hold the other button): spins through a set of food
  options at 20 fps.
- **Release to stop**: letting go stops the roulette on the current frame.
- **Battery readout**: shows the live battery percentage in the top-right corner,
  refreshed every couple of seconds; low battery (<20%) turns red.
- **Auto-poweroff**: after 2 minutes with no key activity the device turns off
  the backlight and enters deep sleep (woken by any key).

## Interaction

Three keys drive the whole app; the top-right corner shows the battery level.

- **UP (hold)**: cycle the lunch-prompt animation.
- **DOWN (hold)**: cycle the food-picker animation.
- **Release**: stop on the current frame.
- **OK (single press)**: toggle between the LVGL partial-redraw renderer and a
  fast local+interlaced renderer (for A/B comparing refresh speed vs. tearing).

## Source

- Repository: `Shinku-Chen/ai-passport`, branch `feature/cheerful-goodall`
  (<https://github.com/Shinku-Chen/ai-passport/tree/feature/cheerful-goodall>).
- Released to the community as project `community-3299e0e9`.
