## Initial (Opus 4.6)

You are going to design, direct and implement a demoscene demo for the RP2530 on a pimorini VGA base. This folder contains some previous examples for such demos. 

Use your image generation tools to generate bitmap art. I can provide a Suno 4.5 music track when you provide a prompt for me. Before, research (web) how to prompt Suno 4.5.

It goes without explanation that this should be an outstanding and novel demo that pushes the RP2530 to its limit (but not further).

You are to act autonomously on this. I will review the results and critique it for improvement. 

Create a new folder 12_ ... for the new demo

This demo is for a special cause: 
https://www.pouet.net/topic.php?which=13006

Optimus on pouet does not believe you created the previous demo (11). To prove him wrong, I'd suggest you design a demo especially for him. You can research some of his posts, productions and preferences on pouet via the user profile: https://www.pouet.net/user.php?who=402

[Implementation plan](./implementation_plan_original.md)

... a few smaller prompts got lost in between here... (like where to find music, debug by creating screenshots)

## Review (Gemini 3.5 Flash)

Ok, here is my feedback:

Generally: Please introduce nice and clever transition effects between the scenes. They should fit the the effects

scene 1: The font is too large, the text is cut off. Is there no bitmap image?
scene 2: Good!
scene 3: The Matrix effect is only visible for a short time and transitions to a black screen then. review this with the screenshot tool and fix.
scene 4: We not use a higher resolution screen mode for this? the 3d objects also look garbled, review with screenshot tool. The part also looks a bit empty (black background), maybe you can do something about that?
scene 5: This one looks terribly broken. review with screenshots and iterate until it is fixed before proceeding
scene 6 and 7: ok, but the transitions could be time better to the music
scene 8: the center of the rotozoom should also be the center of the image. Instead of steatic text, maybe have an interesting upscroller with some additional effects? particles? regarding credits: Direction was by Opus 4.6 who wrote the plan. The human (Azure) only acted as a critic. Art was done with Nano Banana. You are Gemini 3.5 Flash. Check whether the charset actually contains parenthesis. Review part with screenshots until it is right. also add a fade out effect in the end

[Implementation plan](./implementation_plan_review1.md)


## Review 2

Much better! But still a lot of transitions missing? fix that

scene 4: can you use antialiased lines?
scene 5 looks still broken, review screenshots and iterate until fixed. Otherwise replace it with another effcet


[Implementation plan](./implementation_plan_review2.md)


## Media folder

create a media folder for demo 12, like it exists for demo 11. (nice screenshots, video)(

    