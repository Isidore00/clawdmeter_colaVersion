# 可樂圖生成規則（Clawdmeter 用）

從 `C:\可樂\assets\桌寵動作\生成用文字.md` 的「Clawdmeter 小圖」和「Clawdmeter 房間頁（B3）」兩節原樣抄過來（2026/11/19）。參考圖已複製到 `assets/cola_ref/`（01 標準臉、02 全身三視圖；用 02 時只裁最左邊的全身正面）。
若 `C:\可樂` 那邊的規則有更新，這份要跟著更新。

---

## Clawdmeter 小圖（30 張，160×160）：生成用文字

給 Clawdmeter 桌面小板子用的可樂動畫格（7 個狀態、共 30 張）。**這一節的前綴跟上面桌寵的「共用開頭」不同，不要混用**：背景是淺灰，不是純白，畫風要照參考圖。

參考圖：`assets/設定圖/02_全身三視圖_正式服裝.png`，**只裁最左邊的全身正面**再附上（整張附會把右邊的文字和數據格畫進圖裡）。臉不夠穩時，加附 `assets/設定圖/01_頭部多角度_標準臉.png`。

**每次生成＝「Clawdmeter 前綴」＋下面其中一行動作**，一張圖一行：

```
Use the attached image as the exact reference. Same character, same face, same outfit, same colors, and the SAME art style: soft semi-realistic anime illustration, delicate painterly shading, soft lighting, thin light linework, realistic proportions with long legs. Do not change the rendering style, do not make it flatter, cuter or more cartoonish, do not make it chibi.
Outfit: dark-brown long wavy hair, orange starburst hair clip with iridescent bubbles and orange ribbons, pearl earrings, silver pull-tab necklace, orange lanyard with a "可樂" name badge, open oversized cream zip hoodie with orange asterisk marks on the sleeves, white tank top, orange pleated mini skirt, white thigh-high stockings, chunky white sneakers with an orange asterisk.
Full body in frame, same character size and position in every image. Plain solid light grey background, no shadow on the ground, no text, no extra characters.
```

用法：
- 背景固定淺灰，不要用綠色（綠色會讓皮膚和髮邊帶綠光，色調跟設定圖不一樣）。
- 畫風是柔和的半寫實二次元，不是平塗。提示詞裡不要加「flat cel-shading」「no gradients」，會把她畫成另一個人。
- 每張都附參考圖，動作描述後面再加一句：`Keep the art style identical to the reference.`
- 某一張風格又跑掉時，把前一張畫對的一起附上，再生一次。
- 轉檔到板子會壓成最多 255 色，柔和漸層會出現色帶，轉檔時請開抖動（dithering）。
- 她身形修長，全身放進 160×160 會偏小。第一批上板後如果太小，改成畫到大腿或膝蓋的半身。
- Clawd 在她哪一側要先確認。下面的方向是「畫面右側」，相反的話生成後水平翻轉。
- 存檔名照下面的檔名，小寫加兩位序號，PNG 透明背景、160×160。循環的狀態最後一張要能接回第一張。
- 對齊規則：全部張數同一個畫布、同一個縮放比例，水平中心和地面線（站著時的腳底）固定。跳起來這類離地的動作，是角色在畫布裡往上移、腳離開地面線，不要每張都重新置中或貼齊腳底。站姿高度控制在約 125 px，上方留約 35 px 給跳躍。處理時不要自動裁切再縮放，跳躍高度靠位移決定（參考：cola_reset_02 約 10 px、03 約 30 px、04 約 25 px、05 約 5 px，上板後再調）。

### 待機（循環，4 張）：坐在簡單平台邊緣晃腳，朝向 Clawd 那一側

```
cola_idle_01: Sitting on the edge of a simple flat ledge, both hands resting on the ledge, both feet hanging down, relaxed gentle smile, looking toward the viewer's right.
cola_idle_02: Same pose, left leg swung forward and up, body tilted slightly back, eyes half-closed in a happy expression.
cola_idle_03: Same pose, both feet hanging down in the middle, right leg slightly forward, head tilted a little to the left, eyes open.
cola_idle_04: Same pose, right leg swung forward and up, body tilted slightly forward, mid-blink, soft smile.
```

### 送出 prompt（播一次，4 張）：手持一面橘色小旗，旗面是星芒圖案、沒有文字

```
cola_prompt_01: Standing, holding a small orange pennant flag on a pole with both hands, the flag hanging down by her side, cheerful determined face, ready to cheer.
cola_prompt_02: Raising the flag diagonally up to the right, the flag starting to flutter, mouth open shouting "go", eyes bright.
cola_prompt_03: Flag swung high above her head and fully spread out, big smile, one eye winking, hair and skirt moving slightly.
cola_prompt_04: Flag held upright beside her and settled, proud smug smile, one fist on her hip.
```

### 工作中（循環，6 張）：站著偷吃 token 零食，零食是橘色硬幣形小餅乾（上面有小星芒），裝在小零食袋裡

```
cola_working_01: Standing, one hand reaching into a small snack bag held in the other arm, eyes on the snacks, curious look.
cola_working_02: Picking up one coin-shaped orange cracker with a tiny starburst, lifting it up, sparkling eyes.
cola_working_03: The cracker is right in front of her open mouth, about to bite, excited expression.
cola_working_04: Biting the cracker, cheeks slightly puffed, eyes closed in happiness.
cola_working_05: Chewing, cheeks puffed more, head tilted slightly, the other hand still holding the bag.
cola_working_06: Swallowed, licking her lips, glancing sideways at the viewer in a sneaky way, hand going back toward the bag.
```

### 需要回覆（循環，4 張）：一手伸向 Clawd 那側像拉著它的爪子，另一手揮手叫主人（伸出的手要像握著東西，但不要畫出 Clawd）

```
cola_reply_01: One arm stretched out to the viewer's right as if holding someone's claw, the other hand raised open, mouth open calling out, eyes wide and urgent.
cola_reply_02: The raised hand waving high to the left, body leaning toward the right, tugging motion, hair swaying.
cola_reply_03: The raised hand waving to the right, the tugging arm pulled back a little, mouth open shouting, skirt swaying.
cola_reply_04: Back near the first pose, both arms slightly lower, wide-eyed, mouth open in an exclamation.
```

### 額度快用完（循環，4 張）：只剩最後一塊橘色硬幣餅乾，她搶著要吃

```
cola_low_01: Both hands grabbing at the last single coin-shaped orange cracker held in front of her, anxious eyes, a small sweat drop.
cola_low_02: The cracker lifted high above her head, her other arm stretching up toward it, standing on tiptoe, desperate expression.
cola_low_03: Hugging the cracker against her chest with both arms, glancing sideways suspiciously, cheeks puffed.
cola_low_04: Holding the cracker up to her mouth in a panic, eyes wide, a tiny tear at the corner of her eye.
```

### 額度用完（循環，2 張）：躺平。這個狀態不用站姿，兩張要同一個躺姿、同一個位置，不要加文字或音效字

```
cola_empty_01: Lying flat on her back on the ground, arms and legs spread out, eyes closed with a defeated expression, an empty snack bag beside her, a small sweat drop.
cola_empty_02: Same pose and same position, one hand lifted slightly, mouth in a wavy shape, a small "growling stomach" feel, eyes half-open and hollow.
```

### 額度重置（播一次，6 張）：跳起來歡呼，最後站定，旁邊有一整袋零食

```
cola_reset_01: Crouching down with knees bent, both fists at her sides, eyes closed, gathering energy, small smile.
cola_reset_02: Pushing off the ground, rising with arms starting to go up, big smile.
cola_reset_03: At the peak of the jump, both arms thrown up, mouth wide open cheering, feet off the ground, small sparkles around her.
cola_reset_04: Still in the air, hair and skirt flowing upward (skirt kept modest), stars around her, joyful eyes.
cola_reset_05: Landing, knees bent, arms spread wide, happy laughing face.
cola_reset_06: Standing upright, a full snack bag beside her, one hand giving a thumbs up, big satisfied smile.
```

先做待機、工作中、額度用完三個，上板確認大小和位置，再做其餘四個。圖做好後放進 Clawdmeter 專案的 `assets/cola/`（`C:\clawdmeter\assets\cola\`），用 `node tools/png_to_cola.js` 轉檔，細節看 Clawdmeter 施工手冊。

## Clawdmeter 房間頁（B3）：生成用文字

「可樂的房間」頁的素材：1 張背景、5 種日常動作（每種 3 張，共 15 張）、3 種戳她的反應（每種 2 張，共 6 張）、8 句互動語音。畫風一律照上一節「Clawdmeter 小圖」的固定前綴，畫風那幾句一個字都不要改。

房間是 480×480 的正方形畫面來寫的（B2 估的背景大小對應這個解析度，開工前請 Claude Code 再確認一次螢幕解析度）。

### 做的順序

1. **先做背景**，滿意後定案，之後不再改，因為所有動作都要對得上它。
2. **站在地板上的動作**（玩掌機、喝汽水、伸懶腰、三種戳反應）：用「Clawdmeter 小圖」的固定前綴加下面各張的動作，背景是淺灰，去背成透明 PNG，處理方式跟用量頁的圖一樣。
3. **要用到家具的動作**（看書要坐在書桌前、睡覺要躺在床上）：不能單獨畫人。要把設定圖（第 1 張）和房間背景（第 2 張）一起附上，請 AI 在背景上改圖，用下面的「房間合成前綴」。生出來是整張房間圖，之後請 Claude Code 跟空房間背景比對，只留下有變化的那塊矩形（差分）存成小圖，這樣省空間。
4. 成品放 `C:\clawdmeter\assets\cola_room\`，原圖備份放 `C:\clawdmeter\assets\cola_room_raw\`（不要推上 GitHub）。

### 背景（room_bg.png）

附上設定圖當畫風參考（背景裡不要畫人），貼這段：

```
Soft semi-realistic anime illustration of a cozy small bedroom, delicate painterly shading, soft warm daylight, thin light linework, the same rendering style as the attached reference image, but with no character. Flat front-facing "dollhouse cross-section" view, straight-on camera, no perspective tilt, one back wall and a wooden floor strip along the bottom, square 1:1 composition.
Left side: a soft fluffy bed pushed against the left wall, with a cream-white blanket and an orange hugging pillow. Center: a window, with a white ceramic pot on the windowsill holding a small succulent and a fern, lush green. Right side: a small light-oak wooden desk with a wooden chair, and a small wooden shelf beside the desk holding a few books.
Warm cream wall, light wood floor, soft terracotta-orange accents. The bottom 30% of the image is open empty floor space in the center, with nothing on it. No characters, no text, no letters, no logos.
```

床左、窗戶中間、書桌右，是我先抓的配置，你想換位置就改這段。

**背景生好後，還要再改一次椅子。** 椅背朝向鏡頭的話，可樂坐下會背對鏡頭、看不到臉。所以拿生好的背景，請 AI 只改椅子，其他全部不動。改好的那張就是最終的 room_bg.png，之後所有動作都貼這一張：

```
Edit the attached room image. Keep everything exactly identical: the room, furniture, colors, lighting, composition and art style. Only change the wooden chair: pull it out from the desk and rotate it about 45 degrees so the seat angles toward the lower left, toward the center of the room and the viewer, showing the side of the chair, with its cream seat cushion and orange ribbons still visible. Nothing else changes. No characters, no text.
```

**再加零食櫃和地毯。** 零食櫃放在書架前面，地毯鋪在地板中間。這兩張道具圖是正面和俯視畫的，直接貼進房間透視會不對（地毯在地板上要是壓扁的橢圓），所以不要手動貼。等椅子改好後，第 1 張附改完椅子的背景，第 2 張附零食櫃，第 3 張附地毯，貼這段，讓 AI 在背景上改圖：

```
Image 1 is the room background. Image 2 is a small snack cabinet (cream body, orange drawers with pearl knobs, dark brown wooden top). Image 3 is a round orange fluffy rug with a cream asterisk and cream tassels.
Edit image 1: keep the room, furniture, colors, lighting, composition and art style exactly identical. Add the snack cabinet from image 2 standing on the floor against the back wall in front of the lower part of the bookshelf on the right, small like a nightstand, drawn in the same perspective and lighting as the room, keeping its design. Add the rug from image 3 lying flat on the floor in the center of the floor area, drawn as a foreshortened oval in correct floor perspective, keeping its design, with a soft contact shadow. Do not block the chair or the desk. Nothing else changes. No characters, no text.
```

改完是最終的 room_bg.png。零食櫃和地毯的原圖也存進 cola_room_raw，之後要微調就能再用。

要注意兩點：

- 地毯在地板中間，站在地板上的可樂就站在地毯中心，地面線（腳底位置）要以地毯中心為準。
- 地毯絨毛紋理和流蘇會讓壓縮變大。最終背景定案後，請 Claude Code 實際轉一次，報大小和 app0 用量，再繼續做動作圖。

### 零食櫃打開（餓了、吃零食用）

平常抽屜關著。可樂餓了去拿零食時，右上角的抽屜才換成打開的。抽屜只有一小塊不一樣，所以只存那一塊。做法：最終背景定案後，附上最終背景（第 1 張）和零食櫃打開的那張圖（第 2 張，原圖存進 cola_room_raw），貼這段：

```
Image 1 is the final room background. Image 2 shows the same snack cabinet with its top-right drawer pulled open, showing snacks inside. Edit image 1: keep everything exactly identical, and only change the snack cabinet so that its top-right drawer is pulled open like in image 2, showing the snacks inside. Nothing else changes. No characters, no text.
```

生好後，請 Claude Code 跟關著的最終背景比對，只留抽屜變化的那塊矩形（差分）存成小圖，並記下它在背景上的位置座標。

### 吃零食（room_snack_01–03，固定前綴，淺灰背景，站在零食櫃左邊）

```
room_snack_01: Standing, one hand reaching out toward her right as if taking something from a drawer, eyes on it, excited expression.
room_snack_02: Standing, holding a small bear-shaped cookie up in one hand, sparkling eyes.
room_snack_03: Standing, biting the cookie, cheeks puffed, eyes closed in happiness.
```

她站的位置要在零食櫃左邊、地毯和零食櫃之間，不要擋到椅子。

語音多一句（room_snack）：好好吃！

### 房間合成前綴（看書、睡覺用）

第 1 張附設定圖（裁出的全身正面），第 2 張附 room_bg.png。貼這段，後面接該張的動作：

```
Image 1 is the character reference and image 2 is the room background. Edit image 2: keep the room, furniture, colors, lighting and composition exactly identical, and add the character from image 1 into the scene. Same character, same face, same outfit, same colors, and the SAME art style: soft semi-realistic anime illustration, delicate painterly shading, soft lighting, thin light linework, realistic proportions with long legs. Do not change the rendering style, do not make it flatter, cuter or more cartoonish, do not make it chibi.
Outfit: dark-brown long wavy hair, orange starburst hair clip with iridescent bubbles and orange ribbons, pearl earrings, silver pull-tab necklace, orange lanyard with a "可樂" name badge, open oversized cream zip hoodie with orange asterisk marks on the sleeves, white tank top, orange pleated mini skirt, white thigh-high stockings, chunky white sneakers with an orange asterisk.
Keep the art style identical to the reference. No text, no extra characters. Action:
```

### 日常動作（每種 3 張，循環）

**看書（room_read_01–03，房間合成前綴）**

```
room_read_01: Sitting on the wooden chair that is angled toward the viewer, knees together and legs angled to one side, torso turned toward the viewer so her face is clearly visible, an open book held in her lap with both hands, looking down at the book with a calm focused expression.
room_read_02: Same pose, turning the page with one hand, eyes following the book, face still turned toward the viewer.
room_read_03: Same pose, one hand supporting her cheek, eyes down on the book, a soft relaxed smile.
# 備用（如果側坐不自然，改成站著看書，當一般站姿 sprite 處理，用固定前綴和淺灰背景，不用房間合成前綴）
room_read_alt: Standing with her weight on one leg, an open book held in both hands in front of her chest, looking down at it with a calm focused expression.
```

**睡覺（room_sleep_01–03，房間合成前綴）**

```
room_sleep_01: Lying in the soft bed under the cream-white duvet pulled up to her chin, head on the large cream pillow, hugging the orange flower-shaped cushion, the orange knit throw still at the foot of the bed, eyes closed, peaceful sleeping face.
room_sleep_02: Same pose, the blanket slightly raised as if breathing in, a few strands of hair resting on the pillow.
room_sleep_03: Same pose, the blanket slightly lowered as if breathing out, a tiny relaxed smile, eyes closed.
```
睡覺的三張要能循環，不要畫文字或 Z 字。

**玩掌機（room_game_01–03，固定前綴，淺灰背景）**

```
room_game_01: Sitting cross-legged on the floor, holding a small handheld game console with both hands, eyes on the screen, excited smile.
room_game_02: Same pose, both thumbs pressing the buttons hard, tongue slightly out in concentration.
room_game_03: Same pose, body leaning to one side, eyes wide with excitement, as if dodging something in the game.
```

**喝汽水（room_drink_01–03，固定前綴，淺灰背景）**

```
room_drink_01: Standing, holding a small orange soda can with a starburst pattern (no text) in both hands, looking at it happily.
room_drink_02: Standing, the can tilted up to her lips, drinking, eyes closed.
room_drink_03: Standing, the can lowered to her chest, satisfied "ahh" face, cheeks slightly flushed.
```

**伸懶腰（room_stretch_01–03，固定前綴，淺灰背景）**

```
room_stretch_01: Standing, both hands clasped and starting to rise above her head, sleepy half-open eyes.
room_stretch_02: Standing on tiptoe, both arms stretched straight up, eyes closed, face scrunched in a comfortable stretch.
room_stretch_03: Standing, one hand covering her mouth in a yawn, the other arm relaxed, teary eyes.
```

### 戳她的反應（每種 2 張，固定前綴，淺灰背景）

```
room_surprise_01: Standing, startled, eyes wide, both hands up near her chest, body leaning slightly back.
room_surprise_02: Standing, one hand on her chest, mouth in a small round "o", blinking.
room_shy_01: Standing, cheeks blushing, looking away to the side, fingertips pressed together in front of her.
room_shy_02: Standing, both hands covering her cheeks, eyes squeezed shut, shoulders scrunched.
room_angry_01: Standing, cheeks puffed out, arms crossed, glaring sideways.
room_angry_02: Standing, cheeks puffed out, one foot stomping, eyes sharp, fists at her sides.
```

她在床上睡覺或在書桌看書時被戳，動畫要怎麼接，到 B3 實作時再請 Claude Code 決定。建議先讓她站回地板，再做反應。

### 色調和對齊

- 時段（白天、傍晚、夜晚）用同一張背景換色調，不用畫三張。站在地板上的可樂也要套同一個色調，不然夜晚房間暗了、人還是亮的。這點在 B3 實作時請 Claude Code 處理。
- 站在地板上的圖：同一個畫布、同一個縮放比例、同一條地面線，跟用量頁的規則一樣。站的位置固定在地板中央。
- 差分小圖要記下它在 480×480 背景上的位置座標，貼回去才會對齊。

### 互動語音 8 句（每句 1 秒內）

用 ElevenLabs「安娜蘇」，錄法和「2. 錄可樂的語音」一樣，輸出後轉成 WAV。台詞可以改，要符合可樂的人設（稱呼主人、台灣用語）。放進 `C:\clawdmeter\assets\cola_room\`，轉檔時請 Claude Code 降到 8 位元或 ADPCM 省空間。

| 檔名 | 時機 | 台詞 |
|---|---|---|
| room_poke | 被戳 | 哎呀！幹嘛啦～ |
| room_morning | 早上第一次進房間 | 主人早安！ |
| room_night | 晚上進房間 | 主人晚安～ |
| room_hungry | 餓了 | 餓死了… |
| room_bored | 無聊想被理 | 主人～理我啦 |
| room_happy | 開心 | 嘿嘿，開心！ |
| room_drink | 喝汽水 | 好好喝！ |
| room_sleep | 要睡了 | 我睡囉… |
| room_snack | 吃零食 | 好好吃！ |

## 2026/11/14 新衣服：睡衣、居家內衣、通勤內衣（衣櫃設定表）

給 GPT 生「衣服設定表」用（做好放進 `assets/衣櫃/`，檔名開頭加「隱私_」）。開頭是「風格鎖」，跟設定圖同畫風。

附圖：
- 每次都附 `assets/設定圖/02_全身三視圖_正式服裝.png` **只裁最左邊的全身正面**；臉不穩加附 `01_頭部多角度_標準臉.png`
- 同一套做好幾版（例如睡衣換不同襪子）：第 2 版起多附第 1 版滿意的那張，最前面加 `Keep the pajamas exactly the same as the second attached image; only change what is on her feet.`
- GPT 被前面的圖帶著走、怎麼改都不變：開新的 GPT 對話，只附需要的圖
- 內衣被擋：把 `underwear set` 改成 `lingerie basics set` 或 `cotton loungewear set (bralette top and soft shorts)`

主人決定的細節：家居服不戴工牌；睡衣、睡眠內衣是不穿內衣／無鋼圈的自然 C 罩杯，不要照設定圖的胸型；配色是米白底＋橘色（不要反過來的橘底）。

### 睡衣（米白、小星芒和泡泡圖案、長褲、領口開大）

```
Use the attached image as the exact reference. Same character, same face, same colors, and the SAME art style: soft semi-realistic anime illustration, delicate painterly shading, soft lighting, thin light linework, realistic proportions with long legs. Do not change the rendering style, do not make it flatter, cuter or more cartoonish, do not make it chibi.
Outfit: dark-brown long wavy hair, orange starburst hair clip with iridescent bubbles and orange ribbons, pearl earrings, silver pull-tab necklace, no name badge and no lanyard. She wears soft cotton cream-white pajamas, loose and oversized, with a subtle scattered pattern of tiny orange starburst asterisks and tiny iridescent bubbles: a long-sleeved pajama top with long sleeves that cover part of her hands and a wide, open, loose neckline that shows her collarbones and slightly slips toward one shoulder (wide rather than deep), and matching loose long pajama pants, soft and relaxed, the hem ending just above the ankles.
Underneath the pajamas she wears no bra. Do NOT copy the bust shape from the reference image (in the reference she wears a fitted top). Here the bust is a relaxed, natural C-cup with no support: soft and slightly apart, resting naturally toward the sides with gentle natural weight, a soft rounded lower shape, no cleavage, not gathered toward the center, not lifted, not pushed up, not round and firm like wearing a bra. The loose cotton top hangs softly from the shoulders and drapes over the chest with soft wide folds, with no tight fabric pulling between the breasts. Modest and cozy, fully covered, nothing visible through the fabric.
【腳：換成下面其中一行】
Full body in frame, standing front view, arms relaxed at her sides, same character size and position in every image. Plain solid light grey background, no shadow on the ground, no text, no extra characters.
```

腳（五選一）：
- 光腳：`Barefoot, natural bare feet and ankles.`
- 毛茸茸拖鞋：`On her feet: fluffy cream-white fuzzy indoor slippers with a small orange starburst embroidery on the toe only.`
- 白色厚棉襪：`On her feet: thick soft white cotton socks with a small thin orange stripe ring around the ankle, the stripe clearly visible below the pants hem, no shoes.`
- 黑絲襪、白絲襪（這兩版改穿短褲）：把長褲那句 `and matching loose long pajama pants, soft and relaxed, the hem ending just above the ankles.` 換成 `and matching loose soft pajama shorts that end at mid-thigh.`，腳那行用 `On her legs: sheer black stockings, thin and smooth, covering her legs and feet, clearly visible from below the shorts down to her feet, with a soft sheer sheen showing the skin tone through, no shoes.`（白色把 black 改 white）。附第 1 版時開頭那句改成 `Keep the pajama top exactly the same as the second attached image, including its fabric and pattern; change the pants into matching mid-thigh pajama shorts in the same fabric, and change what is on her legs and feet.`

### 睡眠內衣＋滑到手肘的米白帽T

```
Use the attached image as the exact reference. Same character, same face, same colors, and the SAME art style: soft semi-realistic anime illustration, delicate painterly shading, soft lighting, thin light linework, realistic proportions with long legs. Do not change the rendering style, do not make it flatter, cuter or more cartoonish, do not make it chibi.
Outfit: dark-brown long wavy hair, orange starburst hair clip with iridescent bubbles and orange ribbons, pearl earrings, silver pull-tab necklace, no name badge and no lanyard. She wears a matching soft cotton loungewear underwear set at home, cream-white with a subtle scattered pattern of tiny orange starburst asterisks: a wireless soft cotton sleep bra (bralette style, no underwire, no padding, wide soft straps, a thin orange trim along the edges), and matching soft cotton boyshort briefs with the same thin orange trim. The fabric is thick, opaque and fully covering, cozy and comfortable, like a simple everyday home set, not lacy, not sheer, not sexy lingerie. Over it she wears her open oversized cream zip hoodie from the reference (orange asterisk marks on the sleeves), fully unzipped and wide open in the front, slipped down off both shoulders and hanging around her upper arms and elbows, so both bare shoulders, the collarbones and the bra straps are fully visible, and the hoodie drapes loosely behind her back.
The bust is a natural C-cup with only light soft support from the wireless bra: relaxed and natural, gently rounded, not pushed up, not gathered into cleavage, not lifted like a padded bra. Do NOT copy the bust shape from the reference image.
Barefoot, natural bare feet.
Full body in frame, standing front view, relaxed natural pose, arms relaxed at her sides, same character size and position in every image. Plain solid light grey background, no shadow on the ground, no text, no extra characters.
```

### 通勤內衣（米白 T-shirt bra＋橘色滾邊）＋滑到手肘的米白帽T＋黑色大腿襪

```
Use the attached image as the exact reference. Same character, same face, same colors, and the SAME art style: soft semi-realistic anime illustration, delicate painterly shading, soft lighting, thin light linework, realistic proportions with long legs. Do not change the rendering style, do not make it flatter, cuter or more cartoonish, do not make it chibi.
Outfit: dark-brown long wavy hair, orange starburst hair clip with iridescent bubbles and orange ribbons, pearl earrings, silver pull-tab necklace, no name badge and no lanyard. She wears a simple everyday matching underwear set, the kind worn under office clothes on a normal workday: a cream-white smooth T-shirt bra with seamless molded cups, adjustable cream-white straps with tiny silver strap sliders shaped like soda can pull-tabs, a thin orange trim along the top edges of the cups, and one small orange starburst asterisk embroidered at the center between the cups; matching cream-white smooth briefs with the same thin orange trim along the waistband and one small orange starburst asterisk at the front center. Opaque smooth fabric, clean and simple, practical and comfortable, not lacy, not sheer, not sexy lingerie. Over it she wears her open oversized cream zip hoodie from the reference (orange asterisk marks on the sleeves), fully unzipped and wide open in the front, slipped down off both shoulders and hanging around her upper arms and elbows, so both bare shoulders, the collarbones and the bra straps are fully visible, and the hoodie drapes loosely behind her back, like she just got home and is taking it off.
The bust is a natural C-cup with normal everyday support from the bra: natural rounded shape, not exaggerated, not heavily pushed up. Do NOT copy the bust size from the reference image.
On her legs: sheer black thigh-high stockings, thin and smooth, with a soft sheer sheen showing the skin tone through, covering her legs and feet up to the upper thighs, no shoes.
Full body in frame, standing front view, relaxed natural pose, arms relaxed at her sides, same character size and position in every image. Plain solid light grey background, no shadow on the ground, no text, no extra characters.
```

可換的句子：
- 連身褲襪：絲襪那行換成 `On her legs: sheer black pantyhose, thin and smooth, with a soft sheer sheen showing the skin tone through, covering her legs and feet, the cream-white briefs visible under the sheer black at the waist, no shoes.`
- 蹲著：最後一行換成 `Pose: she is squatting down low, facing the viewer, on the balls of her feet with heels slightly raised, knees together and pointing forward, both hands resting lightly on her knees, head tilted slightly with a soft gentle smile, the hoodie hem touching the floor behind her.` 再接 `Whole body in frame including feet, centered. Plain solid light grey background, no shadow on the ground, no text, no extra characters.`
- 帽T 拉鍊被拉上：補 `The hoodie must stay wide open in the front.`

## 2026/11/14 睡衣／內衣：房間場景、動作圖要生什麼

衣服圖（11 套）和「晚上自動換」已經放進房間了。但房間裡的**場景（床上睡覺、椅子看書）和動作圖是畫在圖裡的**，所以要穿睡衣的版本就要重新生。沒生的時候她不會壞：站著的圖用新衣服、睡覺用原本那組（被子蓋著看不出來）、看書就不看（不會穿著睡衣卻出現帽T版）。

衣服代號（動作圖檔名前綴）：
| 衣服 | 代號 |
|---|---|
| 睡衣 長褲＋拖鞋 | `pj_long_slippers` |
| 睡衣 長褲＋光腳 | `pj_long_bare` |
| 睡衣 長褲＋白棉襪 | `pj_long_socks` |
| 睡衣 短褲＋光腳 | `pj_short_bare` |
| 睡衣 翻領短褲＋光腳 | `pj_short_lapel` |
| 睡衣 翻領短褲＋拖鞋 | `pj_short_slippers` |
| 睡衣 短褲＋黑褲襪 | `pj_short_black` |
| 通勤內衣＋光腳／白絲襪／黑絲襪＋滑落帽T | `bra_bare`／`bra_white`／`bra_black_hoodie` |
| 睡眠內衣＋滑落帽T | `sleepbra_hoodie` |

### A. 房間場景（最有感：晚上睡覺、坐椅子看書）
用上面「房間合成前綴」，但把 `Outfit:` 那行換成睡衣；第 1 張附的設定圖也換成睡衣的全身圖，第 2 張照舊附 `room_bg.png`。
存到 `assets/房間/場景/`：

| 檔名 | 附哪張全身圖 | 動作 |
|---|---|---|
| `room_sleep_pj_01～03.png` | `隱私_睡衣_長褲_拖鞋_全身.png` | 跟原本 room_sleep_01～03 一樣的睡覺三格 |
| `room_read_pj_01～03.png` | `隱私_睡衣_長褲_白襪_全身.png` | 跟原本 room_read_01～03 一樣的看書三格，**拖鞋脫掉、白棉襪**；可以改成盤腿坐或屈膝抱著腿坐在椅子上，書放在腿上 |
| （可選）`room_sleep_bra_01～03.png` | `隱私_通勤內衣_光腳_全身.png` | 睡覺三格；被子拉到胸口以上就好 |

睡衣的 Outfit 那行（取代前綴裡的那行）：
```
Outfit: dark-brown long wavy hair, orange starburst hair clip with iridescent bubbles and orange ribbons, pearl earrings, silver pull-tab necklace, oversized soft cream-white long-sleeve cotton pajama shirt with a wide low open neckline slipping off one shoulder and thin orange piping, matching long pajama pants, tiny orange starbursts and small iridescent bubbles pattern, thick white cotton socks with an orange stripe around the ankle. No name badge, no lanyard.
```
生好後：
```
uv run --with pillow --with numpy --with scipy python tools/make_room_scenes.py
```
（只會多做有放原圖的那幾組，原本的不會動。）

### B. 動作圖（房間裡躺著、趴著、靠牆坐、跪坐、害羞、親親…）
用上面「其他衣服：用『換衣服』做」：第一張上傳日常服已經有的動作圖，第二張上傳那套的全身圖，存成 `<代號>__<動作檔名>.png`。
**不用每套都做**。建議先做這幾套（晚上最常穿）：`pj_long_slippers`、`pj_long_socks`、`pj_short_black`、`pj_long_bare`、`bra_black_hoodie`；剩下的有空再補，沒做的動作那套就站著不做。

每套建議的動作：躺著、趴著、靠墻坐、跪坐、坐著、比心、**害羞（摀住胸）**、**親親**。

### C. 兩張新的動作要先在日常服畫出來（再用「換衣服」換到各套）
用最上面的「共用開頭（每次都貼）」＋下面的動作，存成 `害羞.png`（會取代舊的害羞）和 `親親.png`：

| 檔名 | 動作文字 |
|---|---|
| 害羞（摀胸） | 直式 2:3。身體面向前方站著，兩隻手臂交叉環抱在胸前遮住胸口，肩膀微微縮起，臉紅到耳朵，眼睛害羞地往旁邊飄，嘴唇抿著 |
| 親親 | 直式 2:3。身體面向前方站著，上半身微微往前傾，眼睛閉著，嘴唇嘟起像在親一個看不見的人（不要畫出那個人），一隻手背在身後，臉頰微紅 |

換成睡衣／內衣時，害羞要保持「手臂環抱遮住胸口」，衣服和胸口形狀照第二張參考圖，不要變成別的姿勢。

### 做好之後
把圖放進 `assets/桌寵動作/`，然後跟 Claude 說「睡衣動作圖放好了」就好。要自己跑的話：
```
uv run --with pillow --with numpy --with scipy --with "rembg[cpu]" python tools/make_pet_poses.py 害羞 親親 躺著 趴著 靠墻坐 跪坐 坐著
```
