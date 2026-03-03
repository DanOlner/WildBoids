# Actual human notes

## Things to follow up

- ["in which I analyze ANNs when used as scientific instruments of study and when functioning as emergent arbiters of the zeitgeist in the cognitive, computational, and neural sciences."](https://bsky.app/profile/olivia.science/post/3mg5ojrl4p22i)

## Sim log

### 22.2.26

Keeping gen24 from the current run, renamed for commit champion_gen24_e4ad2f827b6cec6a3d722bd396afeab5c06c0df3.json

(So we can look at the config file for world setup).

This boid is doing well at orienting to food source but can't slow enough to stick around, mostly, unless it gets lucky. But it's getting there.

This is also highlighting the the boids have no 'experiment with behaviour when there's no sensor input' going on. They default to static thrust. Though they do still react to fellow boids, I'm not sure that's getting them anywhere. In groups, they spin.

Probably time to set up parameter space searching, as well as testing other aspects are doing what we want. Or possibly jump to predators.