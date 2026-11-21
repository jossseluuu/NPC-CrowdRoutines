// Interactive preview model. The native C++ core remains a separate backend.
const canvas = document.getElementById('map');
const ctx = canvas.getContext('2d');
const W = 500, H = 300;
const activities = ['Sleeping','Breakfast','Commuting','Working','Studying','Care duties','Eating','Shopping','Leisure','Investigating','Talking','Pursuing','Fleeing','Evacuating','Home'];
const activityColors = {Sleeping:'#8093a6',Breakfast:'#ffca70',Commuting:'#67d4cd',Working:'#a9dc79',Studying:'#b89af4','Care duties':'#f09abc',Eating:'#ffca70',Shopping:'#eea66c',Leisure:'#89b9ef',Investigating:'#e8cc72',Talking:'#dc9ce8',Pursuing:'#ff786e',Fleeing:'#f35f75',Evacuating:'#ff525f',Home:'#9db5a1'};
const profileDefs = [
  {id:'worker',name:'Day worker',glyph:'circle',weight:30},
  {id:'shift',name:'Shift worker',glyph:'square',weight:14},
  {id:'student',name:'Student',glyph:'triangle',weight:15},
  {id:'care',name:'Care / household',glyph:'diamond',weight:13},
  {id:'retired',name:'Retired',glyph:'hex',weight:13},
  {id:'flexible',name:'Flexible work',glyph:'cross',weight:15}
];
const streets = ['North Loop','Foundry Avenue','Market Street','Canal Road','Museum Way','Riverside Drive','Terminal Boulevard','Harbor Street','Mill Lane','Garden Road','Station Street','South Quay','Dock Street'];
const workSites = ['Civic Centre','Riverside Offices','Market Arcade','North Clinic','Technical College','Harbor Depot','Central School','Old Mill Works','Garden Campus','Transit Yard'];
const jobNames = {worker:['Office','Retail','Public service','Healthcare','Hospitality'],shift:['Nurse','Transit operator','Security','Logistics','Emergency services'],student:['Secondary student','University student','Apprentice'],care:['Caregiver','Household manager','Part-time worker'],retired:['Retired','Volunteer','Consultant'],flexible:['Designer','Freelancer','Remote analyst','Contractor','Small business']};
let agents = [], events = [], sessions = new Map(), grid = new Map(), filters = [], matchingIds = new Set();
let running = true, tick = 0, hour = 7.7, weekDay = 1, worldDay = 18, speed = 10, selected = null, pop = 2500;
let last = performance.now(), lastUi = 0, sessionSerial = 0, rnd = makeRandom(42);
const stepTimes = []; // measured wall time of recent step() calls, in ms

function makeRandom(seed) { let s = seed >>> 0; return () => { s = (s * 1664525 + 1013904223) >>> 0; return s / 4294967296; }; }
function randInt(max) { return Math.floor(rnd() * max); }
function pick(items) { return items[randInt(items.length)]; }
function weighted(items) { let n = rnd() * items.reduce((a,x)=>a+x.weight,0); for(const x of items){n-=x.weight;if(n<=0)return x} return items.at(-1); }
function clamp(v,a,b){return Math.max(a,Math.min(b,v));}
function hourDistance(a,b){return Math.abs(((a-b+36)%24)-12);}
function inSpan(h,start,end){return start<=end ? h>=start&&h<end : h>=start||h<end;}
function untilHour(from,to){return (to-from+24)%24;}
function fmtHour(h){const hh=Math.floor(h)%24, mm=Math.floor((h-Math.floor(h))*60);return `${String(hh).padStart(2,'0')}:${String(mm).padStart(2,'0')}`;}
function homeDistrict(x){return x<125?'A':x<250?'B':x<375?'C':'D';}

function createAgents(count){
  agents=[]; sessions.clear(); sessionSerial=0;
  let id=0, householdId=0;
  while(id<count){
    const householdSize=Math.min(count-id,1+randInt(4));
    const home={x:15+rnd()*390,y:15+rnd()*270};
    const household=[];
    for(let m=0;m<householdSize;m++,id++){
      const profile=weighted(profileDefs);
      const sex=rnd()<.495?'Man':rnd()<.99?'Woman':'Non-binary';
      let age=profile.id==='student'?16+randInt(15):profile.id==='retired'?64+randInt(27):profile.id==='care'?24+randInt(40):19+randInt(47);
      const job=pick(jobNames[profile.id]);
      const workSite=profile.id==='retired'?'—':pick(workSites);
      const siteIndex=workSites.indexOf(workSite),siteBase={x:42+(siteIndex%5)*76,y:34+Math.floor(siteIndex/5)*132};
      const workplace=profile.id==='retired'?{x:home.x,y:home.y}:{x:siteBase.x+(rnd()-.5)*13,y:siteBase.y+(rnd()-.5)*13};
      const mode=pick(['Walk','Bus','Metro','Bicycle','Car']);
      const commuteHours=mode==='Walk'?.42:mode==='Bicycle'?.25:mode==='Car'?.18:.34;
      let workStart=profile.id==='shift'?pick([5,6,13,14,21,22])+rnd()*.6:profile.id==='student'?7.6+rnd()*2.3:profile.id==='care'?8.2+rnd()*3:7.3+rnd()*3.1;
      const workLength=profile.id==='shift'?7.5+rnd()*1.5:profile.id==='student'?4.5+rnd()*2:profile.id==='care'?3+rnd()*4:6.5+rnd()*2.5;
      const workEnd=(workStart+workLength)%24;
      const wakeHour=clamp(workStart-commuteHours-(.55+rnd()*.9),4.4,10.8);
      const sleepStart=(wakeHour+14.4+rnd()*2.2)%24;
      const hasBreakfast=rnd()<(profile.id==='retired'?.82:.72);
      const breakfastVenue=pick(['Home','Cafe','Bakery','Workplace']);
      const breakfastHour=(wakeHour+.25+rnd()*.9)%24;
      const workdays=profile.id==='retired'?[]:profile.id==='shift'?Array.from({length:7},(_,d)=>d).filter(()=>rnd()<.58):[0,1,2,3,4].filter(()=>rnd()<.86);
      const errandChance=profile.id==='retired'?.45:.34;
      const a={
        id,x:home.x+(rnd()-.5)*2.2,y:home.y+(rnd()-.5)*2.2,home:{...home},workplace,tx:home.x,ty:home.y,
        sex,profile:profile.id,profileName:profile.name,glyph:profile.glyph,age,job,workSite,mode,commuteHours,
        workStart,workEnd,workLength,workdays,wakeHour,sleepStart,hasBreakfast,breakfastVenue,breakfastHour,
        breakfastDoneDay:-1,breakfastUntil:0,errandChance,errandDay:-1,errandHour:17+rnd()*5,householdId,
        energy:.4+rnd()*.6,hunger:rnd()*.48,hydration:rnd()*.45,hygiene:.45+rnd()*.55,socialNeed:rnd()*.7,bladder:rnd()*.4,comfort:rnd(),stress:rnd()*.2,
        health:.68+rnd()*.32,fatigue:rnd()*.35,fear:rnd()*.25,curiosity:rnd(),boredom:rnd()*.5,money:rnd(),temperature:.5,pain:rnd()*.12,
        sociability:rnd(),riskAversion:rnd(),patience:rnd(),aggression:rnd()*.55,empathy:rnd(),trust:rnd(),impulsivity:rnd(),localKnowledge:rnd(),activity:'Home',
        relationships:new Map(),interactionId:null,interactionCooldown:0,streetVisits:new Set(),lastStreet:'Off street',eventHistory:new Set(),
        route:[],routeIndex:0,routeGoal:null,nextRepath:0,
        district:homeDistrict(home.x),siteId:workSite==='—'?-1:workSites.indexOf(workSite),safePoint:{x:clamp(home.x+(rnd()-.5)*110,15,410),y:clamp(home.y+(rnd()-.5)*110,15,285)}
      };
      agents.push(a); household.push(a);
    }
    for(const member of household)member.householdSize=household.length;
    for(let i=0;i<household.length;i++)for(let j=i+1;j<household.length;j++)link(household[i],household[j],'family',.75+rnd()*.25);
    householdId++;
  }
  createSocialNetwork();
  for(const a of agents){const road=streetAt(a.x,a.y);if(road){a.streetVisits.add(road);a.lastStreet=road;}}
  updateFilterOptions();applyFilters();
}

function link(a,b,kind,bond){a.relationships.set(b.id,{kind,bond});b.relationships.set(a.id,{kind,bond});}
function createSocialNetwork(){
  for(const a of agents){
    const links=2+randInt(5);
    for(let i=0;i<links;i++){
      const b=agents[randInt(agents.length)]; if(a===b||a.relationships.has(b.id))continue;
      const kind=rnd()<.11?'rival':rnd()<.58?'friend':rnd()<.72?'coworker':'neighbour';
      link(a,b,kind,kind==='rival'?rnd()*.3:.35+rnd()*.6);
    }
  }
}

function streetAt(x,y){
  const vx=Math.round(x/63),hy=Math.round(y/57),dv=Math.abs(x-vx*63),dh=Math.abs(y-hy*57);
  if(Math.min(dv,dh)>5)return null;
  if(dv<dh&&vx<=6)return streets[vx];
  return streets[7+Math.min(5,hy)];
}
function placeFor(a,activity){
  if(activity==='Breakfast'){
    if(a.breakfastVenue==='Home')return a.home;
    if(a.breakfastVenue==='Workplace')return a.workplace;
    return a.breakfastVenue==='Cafe'?{x:145,y:214}:{x:274,y:83};
  }
  if(activity==='Working'||activity==='Studying'||activity==='Commuting')return a.workplace;
  if(activity==='Shopping'||activity==='Eating')return {x:185,y:158};
  if(activity==='Investigating'){const dispute=events.find(e=>e.type==='Street dispute');if(dispute)return{x:dispute.x,y:dispute.y};}
  if(activity==='Leisure')return pickLeisurePlace(a);
  if(activity==='Care duties')return a.home;
  if(activity==='Evacuating')return a.safePoint;
  return a.home;
}
function pickLeisurePlace(a){return a.socialNeed>.68?{x:145,y:214}:a.curiosity>.75?{x:335,y:156}:{x:130+(a.id%4)*38,y:205};}

// Uniform spatial grid stored as linked lists in typed arrays (no per-tick allocations).
const CELL=6,GCOLS=Math.ceil(W/CELL)+1,GROWS=Math.ceil(H/CELL)+1;
let gridHead=new Int32Array(GCOLS*GROWS),gridNext=new Int32Array(0);
function cellIndex(x,y){return clamp(Math.floor(y/CELL),0,GROWS-1)*GCOLS+clamp(Math.floor(x/CELL),0,GCOLS-1);}
function buildGrid(){gridHead.fill(-1);if(gridNext.length<agents.length)gridNext=new Int32Array(agents.length);for(let i=0;i<agents.length;i++){const c=cellIndex(agents[i].x,agents[i].y);gridNext[i]=gridHead[c];gridHead[c]=i;}}
function nearby(a,radius){const out=[],cr=Math.ceil(radius/CELL),cx=Math.floor(a.x/CELL),cy=Math.floor(a.y/CELL),r2=radius*radius;
  for(let y=Math.max(0,cy-cr);y<=Math.min(GROWS-1,cy+cr);y++)for(let x=Math.max(0,cx-cr);x<=Math.min(GCOLS-1,cx+cr);x++)for(let i=gridHead[y*GCOLS+x];i!==-1;i=gridNext[i]){const b=agents[i];if(b!==a){const dx=a.x-b.x,dy=a.y-b.y;if(dx*dx+dy*dy<=r2)out.push(b);}}
  return out;}
// Local separation push. Neighbour count is capped so dense clusters (offices, homes) stay O(1) per agent.
function separation(a,radius,maxNeighbours){let sx=0,sy=0,seen=0;const cx=Math.floor(a.x/CELL),cy=Math.floor(a.y/CELL),r2=radius*radius;
  for(let y=Math.max(0,cy-1);y<=Math.min(GROWS-1,cy+1);y++)for(let x=Math.max(0,cx-1);x<=Math.min(GCOLS-1,cx+1);x++)for(let i=gridHead[y*GCOLS+x];i!==-1;i=gridNext[i]){const b=agents[i];if(b===a)continue;if(++seen>maxNeighbours)return[sx,sy];const qx=a.x-b.x,qy=a.y-b.y,q=qx*qx+qy*qy;if(q>.05&&q<r2){sx+=qx*.025;sy+=qy*.025;}}
  return[sx,sy];}

const roadXs=Array.from({length:7},(_,i)=>i*63),roadYs=Array.from({length:6},(_,i)=>i*57);
function closestNode(p){let ix=0,iy=0,dx=Infinity,dy=Infinity;for(let i=0;i<roadXs.length;i++)if(Math.abs(p.x-roadXs[i])<dx){dx=Math.abs(p.x-roadXs[i]);ix=i;}for(let i=0;i<roadYs.length;i++)if(Math.abs(p.y-roadYs[i])<dy){dy=Math.abs(p.y-roadYs[i]);iy=i;}return iy*roadXs.length+ix;}
function nodePoint(id){return{x:roadXs[id%7],y:roadYs[Math.floor(id/7)]};}
function blockedEdge(a,b){const p=nodePoint(a),q=nodePoint(b),mx=(p.x+q.x)/2,my=(p.y+q.y)/2;return events.some(e=>e.type==='Traffic disruption'&&Math.hypot(mx-e.x,my-e.y)<e.radius*.72);}
function planRoute(agent,target){
  if(Math.hypot(agent.x-target.x,agent.y-target.y)<4){agent.route=[{x:target.x,y:target.y}];agent.routeIndex=0;agent.routeGoal={x:target.x,y:target.y};agent.nextRepath=tick+260;return;}
  const start=closestNode(agent),goal=closestNode(target),n=42,dist=Array(n).fill(Infinity),prev=Array(n).fill(-1),seen=Array(n).fill(false);dist[start]=0;
  for(let k=0;k<n;k++){let u=-1;for(let i=0;i<n;i++)if(!seen[i]&&(u<0||dist[i]<dist[u]))u=i;if(u<0||!Number.isFinite(dist[u])||u===goal)break;seen[u]=true;
    const x=u%7,y=Math.floor(u/7),neighbors=[];if(x>0)neighbors.push(u-1);if(x<6)neighbors.push(u+1);if(y>0)neighbors.push(u-7);if(y<5)neighbors.push(u+7);
    for(const v of neighbors){if(blockedEdge(u,v))continue;const p=nodePoint(u),q=nodePoint(v),cost=Math.hypot(p.x-q.x,p.y-q.y);if(dist[u]+cost<dist[v]){dist[v]=dist[u]+cost;prev[v]=u;}}
  }
  let path=[];if(start!==goal&&prev[goal]===-1)path=[start,goal];else{for(let u=goal;u!==-1;u=prev[u]){path.push(u);if(u===start)break;}path.reverse();}
  agent.route=path.map(nodePoint);agent.route.push({x:target.x,y:target.y});agent.routeIndex=0;agent.routeGoal={x:target.x,y:target.y};agent.nextRepath=tick+((agent.activity==='Pursuing'||agent.activity==='Fleeing')?18:260);
}
function getWaypoint(agent,target){if(!agent.routeGoal||Math.hypot(agent.routeGoal.x-target.x,agent.routeGoal.y-target.y)>8||tick>=agent.nextRepath)planRoute(agent,target);while(agent.routeIndex<agent.route.length-1&&Math.hypot(agent.x-agent.route[agent.routeIndex].x,agent.y-agent.route[agent.routeIndex].y)<3)agent.routeIndex++;return agent.route[agent.routeIndex]||target;}

function startSession(a,b,kind){
  if(a.interactionId||b.interactionId)return;
  const id=++sessionSerial,session={id,a:a.id,b:b.id,kind,phase:kind==='pursuit'?'active':'approach',start:tick,end:tick+(kind==='pursuit'?180+randInt(300):600+randInt(1200)),mid:{x:(a.x+b.x)/2,y:(a.y+b.y)/2}};
  sessions.set(id,session);a.interactionId=id;b.interactionId=id;a.interactionCooldown=tick+900+randInt(900);b.interactionCooldown=a.interactionCooldown;
}
function finishSession(session){const a=agents[session.a],b=agents[session.b];if(a&&a.interactionId===session.id){a.interactionId=null;if(session.kind==='conversation')a.socialNeed=Math.max(0,a.socialNeed-.2);}
  if(b&&b.interactionId===session.id){b.interactionId=null;if(session.kind==='conversation')b.socialNeed=Math.max(0,b.socialNeed-.2);}
  if(session.kind==='conversation'&&a&&b){const ra=a.relationships.get(b.id),rb=b.relationships.get(a.id);if(ra&&rb){ra.bond=Math.min(1,ra.bond+.015);rb.bond=Math.min(1,rb.bond+.015);}}
  sessions.delete(session.id);
}
function processEncounters(){
  for(const session of [...sessions.values()]){
    const a=agents[session.a],b=agents[session.b];if(!a||!b||tick>session.end){finishSession(session);continue;}
    const d=Math.hypot(a.x-b.x,a.y-b.y);
    if(session.phase==='approach'&&d<8){session.phase='active';session.mid={x:(a.x+b.x)/2,y:(a.y+b.y)/2};}
  }
  // Only related pairs can interact, so walk each agent's (small) relationship list
  // instead of scanning every neighbour within 24 units.
  const ENCOUNTER_STRIDE=4;
  for(let ai=tick%ENCOUNTER_STRIDE;ai<agents.length;ai+=ENCOUNTER_STRIDE){const a=agents[ai];
    if(a.interactionId||tick<a.interactionCooldown||a.activity==='Evacuating')continue;
    for(const [bid,relation] of a.relationships){
      const b=agents[bid];
      if(!b||b.id<=a.id||b.interactionId||b.activity==='Evacuating'||tick<b.interactionCooldown)continue;
      const d=Math.hypot(a.x-b.x,a.y-b.y);if(d>24)continue;
      if(relation.kind==='rival'&&d<13&&(a.aggression+b.aggression)>1.05&&(1-(a.empathy+b.empathy)/2)>.35&&rnd()<.018*ENCOUNTER_STRIDE){
        const pursuer=a.aggression>=b.aggression?a:b,runner=pursuer===a?b:a;startSession(pursuer,runner,'pursuit');break;
      }
      if(relation.kind!=='rival'&&d<19&&(a.socialNeed>.52||b.socialNeed>.52)&&rnd()<.012*ENCOUNTER_STRIDE){startSession(a,b,'conversation');break;}
    }
  }
}

function getSession(a){return a.interactionId?sessions.get(a.interactionId):null;}
function chooseActivity(a){
  const localEvent=events.find(e=>Math.hypot(a.x-e.x,a.y-e.y)<e.radius);
  if(localEvent&&localEvent.type==='Evacuation alarm')return 'Evacuating';
  const interaction=getSession(a);
  if(interaction){if(interaction.kind==='conversation'&&interaction.phase==='active')return 'Talking';if(interaction.kind==='pursuit')return interaction.a===a.id?'Pursuing':'Fleeing';}
  if(localEvent&&localEvent.type==='Street dispute'&&a.curiosity>.72&&a.fear<.42)return 'Investigating';
  if(inSpan(hour,a.sleepStart,a.wakeHour)&&a.energy>.12)return 'Sleeping';
  if(a.hasBreakfast&&a.breakfastDoneDay!==worldDay&&hourDistance(hour,a.breakfastHour)<.18){a.breakfastDoneDay=worldDay;a.breakfastUntil=(hour+.22+rnd()*.18)%24;}
  if(a.hasBreakfast&&a.breakfastDoneDay===worldDay&&inSpan(hour,a.breakfastHour,a.breakfastUntil))return 'Breakfast';
  const worksToday=a.workdays.includes(weekDay);
  if(worksToday&&inSpan(hour,a.workStart,a.workEnd))return a.profile==='student'?'Studying':a.profile==='care'?'Care duties':'Working';
  const untilStart=untilHour(hour,a.workStart);
  if(worksToday&&untilStart<=a.commuteHours&&untilStart>.015)return 'Commuting';
  const sinceEnd=untilHour(a.workEnd,hour);
  if(worksToday&&sinceEnd<=a.commuteHours&&sinceEnd>.015)return 'Commuting';
  if(a.errandDay!==worldDay&&rnd()<a.errandChance*.0004&&hour>9&&hour<20){a.errandDay=worldDay;a.errandHour=hour;}
  if(a.errandDay===worldDay&&hourDistance(hour,a.errandHour)<.5)return rnd()<.5?'Shopping':'Eating';
  if(hour>=17&&hour<22&&(a.socialNeed>.66||a.curiosity>.84))return 'Leisure';
  if(a.hunger>.82&&hour>8&&hour<22)return 'Eating';
  return 'Home';
}

// Event placement: with "Place on map" enabled, the button arms the event and the next map click sets its position.
let armedEvent=null;
function inject(type,x,y){
  const pickOnMap=document.getElementById('placeOnMap')?.checked;
  if(x===undefined&&pickOnMap){armedEvent=type;document.getElementById('armedHint').textContent=`Click the map to place: ${type}`;canvas.classList.add('armed');return;}
  armedEvent=null;const hint=document.getElementById('armedHint');if(hint)hint.textContent='';canvas.classList.remove('armed');
  const event={type,x:x??45+rnd()*350,y:y??22+rnd()*245,radius:type==='Evacuation alarm'?76:type==='Street dispute'?34:48,end:tick+850};events.push(event);addFeed(type);
  if(type==='Street dispute'){
    const nearbyAgents=agents.filter(a=>Math.hypot(a.x-event.x,a.y-event.y)<event.radius);
    if(nearbyAgents.length>1){const a=nearbyAgents[0],b=nearbyAgents.find(x=>x!==a);if(a.aggression>.22)startSession(a,b,'pursuit');else startSession(a,b,'conversation');}
  }
  draw();updateUi();
}
function addFeed(type){const feed=document.getElementById('feed');if(feed.children.length===1&&feed.textContent.includes('No events'))feed.innerHTML='';const row=document.createElement('div');row.className='feedrow';row.innerHTML=`<span class="feedtime">${fmtHour(hour)}</span><span>${type} detected · local responses evolving</span>`;feed.prepend(row);while(feed.children.length>6)feed.lastChild.remove();}

function step(dt){
  tick++;const oldDay=worldDay;hour+=dt*speed*.00017;if(hour>=24){const days=Math.floor(hour/24);hour%=24;worldDay+=days;weekDay=(weekDay+days)%7;}
  events=events.filter(e=>tick<e.end);if(worldDay!==oldDay)for(const a of agents){a.breakfastDoneDay=-1;a.errandDay=-1;}
  buildGrid();processEncounters();
  for(const a of agents){
    a.hunger=clamp(a.hunger+dt*.000038,0,1);a.hydration=clamp(a.hydration+dt*.000027,0,1);a.hygiene=clamp(a.hygiene-dt*.000004,0,1);a.socialNeed=clamp(a.socialNeed+dt*.000016,0,1);a.bladder=clamp(a.bladder+dt*.000012,0,1);a.boredom=clamp(a.boredom+dt*.000008,0,1);
    a.energy=clamp(a.energy+(a.activity==='Sleeping'?.00012:-.000018)*dt,0,1);a.fatigue=clamp(a.fatigue+(a.activity==='Sleeping'?-.00012:.00001)*dt,0,1);
    const nearEvent=events.find(e=>Math.hypot(a.x-e.x,a.y-e.y)<e.radius);
    if(nearEvent){a.stress=clamp(a.stress+dt*(nearEvent.type==='Evacuation alarm'?.00032:nearEvent.type==='Street dispute'?.0002:.0001),0,1);a.eventHistory.add(nearEvent.type);a.fear=clamp(a.fear+dt*(nearEvent.type==='Evacuation alarm'?.00018:.00004),0,1);}
    else a.stress=Math.max(0,a.stress-dt*.000018);
    a.activity=chooseActivity(a);
    const session=getSession(a);let target=placeFor(a,a.activity);
    if(a.activity==='Commuting'){const sinceEnd=untilHour(a.workEnd,hour);if(sinceEnd<=a.commuteHours&&sinceEnd>.015)target=a.home;}
    if(session){
      const other=agents[session.a===a.id?session.b:session.a];
      if(session.kind==='conversation')target=session.phase==='approach'?session.mid:{x:session.mid.x+(a.id===session.a?-3:3),y:session.mid.y};
      if(session.kind==='pursuit')target=session.a===a.id?{x:other.x,y:other.y}:{x:clamp(a.x+(a.x-other.x)*2,8,420),y:clamp(a.y+(a.y-other.y)*2,8,290)};
    }
    if(a.activity==='Evacuating')target=a.safePoint;
    if(a.activity==='Breakfast'){if(a.breakfastVenue==='Home')a.hunger=Math.max(0,a.hunger-dt*.00045);else a.hunger=Math.max(0,a.hunger-dt*.00035);}
    if(a.activity==='Eating')a.hunger=Math.max(0,a.hunger-dt*.00028);
    if(a.activity==='Sleeping'){a.energy=clamp(a.energy+dt*.0001,0,1);a.stress=Math.max(0,a.stress-dt*.00004);a.hygiene=clamp(a.hygiene+dt*.000005,0,1);}
    if(a.activity==='Eating'||a.activity==='Breakfast')a.hydration=Math.max(0,a.hydration-dt*.00012);
    const waypoint=getWaypoint(a,target);let dx=waypoint.x-a.x,dy=waypoint.y-a.y;
    const [sx,sy]=separation(a,4.69,24);
    dx+=sx;dy+=sy;const dist=Math.hypot(dx,dy);
    if(dist>1){const modeSpeed={Walk:.010,Bus:.013,Metro:.015,Bicycle:.014,Car:.014}[a.mode]||.01;const rain=events.some(e=>e.type==='Heavy rain')?.82:1;const speedFactor=a.activity==='Pursuing'?1.3:a.activity==='Fleeing'?1.45:1;const movement=Math.min(dist,dt*modeSpeed*rain*speedFactor);a.x=clamp(a.x+dx/dist*movement,3,423);a.y=clamp(a.y+dy/dist*movement,3,297);}
    const currentStreet=streetAt(a.x,a.y);if(currentStreet){a.lastStreet=currentStreet;a.streetVisits.add(currentStreet);}
  }
}

const profileColors={worker:'#a9dc79',shift:'#67d4cd',student:'#b89af4',care:'#f09abc',retired:'#ffca70',flexible:'#89b9ef'};
const sexColors={Man:'#67b8f0',Woman:'#f08fb6','Non-binary':'#c8e07a'};
// Static city layer (ground, blocks, park, roads, labels) is rendered once and blitted every frame.
let cityLayer=null;
function renderCityLayer(){
  const layer=document.createElement('canvas');layer.width=canvas.width;layer.height=canvas.height;const c=layer.getContext('2d');
  c.fillStyle='#172521';c.fillRect(0,0,layer.width,layer.height);c.save();c.scale(2.2,2.2);
  c.fillStyle='#26352e';c.fillRect(0,0,W,H+3);c.fillStyle='#172b30';c.beginPath();c.moveTo(405,0);c.lineTo(500,0);c.lineTo(500,303);c.lineTo(466,303);c.lineTo(440,215);c.lineTo(458,145);c.closePath();c.fill();
  for(let y=20;y<300;y+=57)for(let x=20;x<400;x+=63){const w=40+(Math.floor(x/63)%2)*9,h=29+(Math.floor(y/57)%2)*8;c.fillStyle='#34433b';c.fillRect(x,y,w,h);c.fillStyle='#3c4c42';c.fillRect(x+3,y+3,w-6,3);c.fillStyle='#2c3832';for(let bx=x+8;bx<x+w-4;bx+=11)for(let by=y+11;by<y+h-3;by+=10)c.fillRect(bx,by,4,3);}
  c.fillStyle='#203b31';c.fillRect(117,197,53,35);c.fillStyle='#40513b';for(let i=0;i<14;i++){c.beginPath();c.arc(123+(i*29%43),202+(i*17%25),2.8,0,Math.PI*2);c.fill();}
  c.strokeStyle='#65716e';c.lineWidth=7;for(let x=0;x<=420;x+=63){c.beginPath();c.moveTo(x,0);c.lineTo(x,303);c.stroke();}for(let y=0;y<=300;y+=57){c.beginPath();c.moveTo(0,y);c.lineTo(425,y);c.stroke();}
  c.strokeStyle='#84908b';c.lineWidth=1;c.setLineDash([5,6]);for(let x=0;x<=420;x+=63){c.beginPath();c.moveTo(x,0);c.lineTo(x,303);c.stroke();}for(let y=0;y<=300;y+=57){c.beginPath();c.moveTo(0,y);c.lineTo(425,y);c.stroke();}c.setLineDash([]);
  c.font='4px sans-serif';c.fillStyle='#a1aaa2';for(let x=8;x<420;x+=63)c.fillText(streets[Math.min(6,Math.round(x/63))],x,6);
  for(let i=0;i<workSites.length;i++){const sx=42+(i%5)*76,sy=34+Math.floor(i/5)*132;c.fillStyle='#65d6d233';c.beginPath();c.arc(sx,sy,4,0,Math.PI*2);c.fill();c.fillStyle='#8fb7b3';c.font='3.4px sans-serif';c.fillText(workSites[i],sx-12,sy-6);}
  c.restore();return layer;
}
function agentColor(a,mode){
  if(mode==='stress')return a.stress>.65?'#ff6572':a.stress>.35?'#ffb463':'#66cdbb';
  if(mode==='needs')return a.hunger>.7?'#ffc46f':a.energy<.25?'#bd9aff':a.socialNeed>.75?'#f09abc':'#69cfc8';
  if(mode==='profile')return profileColors[a.profile]||'#a9dc79';
  if(mode==='sex')return sexColors[a.sex]||'#a9dc79';
  return activityColors[a.activity]||'#a9dc79';
}
function drawCity(c){
  if(!cityLayer)cityLayer=renderCityLayer();
  c.clearRect(0,0,canvas.width,canvas.height);c.drawImage(cityLayer,0,0);c.save();c.scale(2.2,2.2);
  for(const e of events){c.beginPath();c.arc(e.x,e.y,e.radius,0,Math.PI*2);c.fillStyle=e.type==='Evacuation alarm'?'#ff505018':e.type==='Street dispute'?'#ff806018':e.type==='Heavy rain'?'#5aa8ff14':'#ffb35a17';c.fill();c.strokeStyle=e.type==='Evacuation alarm'?'#ff767055':e.type==='Heavy rain'?'#75b6ed55':'#ffbb6650';c.lineWidth=1;c.stroke();}
  for(const s of sessions.values()){const a=agents[s.a],b=agents[s.b];if(!a||!b)continue;c.beginPath();c.moveTo(a.x,a.y);c.lineTo(b.x,b.y);c.strokeStyle=s.kind==='pursuit'?'#ff777988':'#e8a7ec77';c.lineWidth=.7;c.stroke();}
  const colorMode=document.getElementById('colorMode').value,symbolMode=document.getElementById('symbolMode').value,small=agents.length>9000?.95:1.25;
  if(selected!==null){const a=agents[selected];if(a&&a.route.length){c.beginPath();c.moveTo(a.x,a.y);for(let i=a.routeIndex;i<a.route.length;i++)c.lineTo(a.route[i].x,a.route[i].y);c.strokeStyle='#ffffff66';c.setLineDash([2,2]);c.lineWidth=.6;c.stroke();c.setLineDash([]);}}
  // Residents are filled only (no per-agent stroke), and canvas state changes only when colour/alpha changes.
  let sel=null,lastColor='',lastAlpha=-1;c.lineWidth=.6;
  for(const a of agents){if(a.id===selected){sel=a;continue;}const color=agentColor(a,colorMode),alpha=filters.length===0||matchingIds.has(a.id)?.88:.10;
    if(alpha!==lastAlpha){c.globalAlpha=alpha;lastAlpha=alpha;}if(color!==lastColor){c.fillStyle=color;c.strokeStyle=color;lastColor=color;}
    const glyph=symbolMode==='sex'?genderGlyph(a.sex):a.glyph;c.beginPath();addGlyph(c,a.x,a.y,glyph==='cross'?small*1.2:small+.2,glyph);if(glyph==='cross')c.stroke();else c.fill();}
  if(sel){c.globalAlpha=1;const color=agentColor(sel,colorMode);c.fillStyle=color;c.strokeStyle='#ffffff';c.lineWidth=1.1;drawGlyph(c,sel.x,sel.y,2.35,symbolMode==='sex'?genderGlyph(sel.sex):sel.glyph);c.beginPath();c.arc(sel.x,sel.y,4.5,0,Math.PI*2);c.stroke();}
  c.globalAlpha=1;c.restore();
}
function genderGlyph(sex){return sex==='Man'?'circle':sex==='Woman'?'diamond':'hex';}
function addGlyph(p,x,y,r,type){if(type==='circle'){p.moveTo(x+r,y);p.arc(x,y,r,0,Math.PI*2);}else if(type==='square')p.rect(x-r,y-r,r*2,r*2);else if(type==='triangle'){p.moveTo(x,y-r*1.25);p.lineTo(x+r*1.1,y+r);p.lineTo(x-r*1.1,y+r);p.closePath();}else if(type==='diamond'){p.moveTo(x,y-r*1.3);p.lineTo(x+r,y);p.lineTo(x,y+r*1.3);p.lineTo(x-r,y);p.closePath();}else if(type==='hex'){for(let i=0;i<6;i++){const angle=i*Math.PI/3-Math.PI/6,px=x+Math.cos(angle)*r,py=y+Math.sin(angle)*r;if(i===0)p.moveTo(px,py);else p.lineTo(px,py);}p.closePath();}else{p.moveTo(x-r,y);p.lineTo(x+r,y);p.moveTo(x,y-r);p.lineTo(x,y+r);}}
function drawGlyph(c,x,y,r,type){c.beginPath();addGlyph(c,x,y,r,type);c.fill();c.stroke();}
function draw(){drawCity(ctx);}
function legendEntriesForColor(mode){
  if(mode==='stress')return [['Calm','#66cdbb'],['Tense','#ffb463'],['High stress','#ff6572']];
  if(mode==='needs')return [['OK','#69cfc8'],['Hungry','#ffc46f'],['Exhausted','#bd9aff'],['Lonely','#f09abc']];
  if(mode==='profile')return profileDefs.map(p=>[p.name,profileColors[p.id]]);
  if(mode==='sex')return Object.entries(sexColors);
  return ['Sleeping','Working','Commuting','Home','Leisure','Talking','Fleeing','Evacuating'].map(k=>[k,activityColors[k]]);
}
function updateLegend(){const mode=document.getElementById('symbolMode').value,legend=document.getElementById('legend');const entries=mode==='sex'?[['Man','circle'],['Woman','diamond'],['Non-binary','hex']]:profileDefs.map(p=>[p.name,p.glyph]);
  const colors=legendEntriesForColor(document.getElementById('colorMode').value);
  legend.innerHTML=`<div class="legendrow">${entries.map(([name,glyph])=>`<span class="legenditem"><canvas width="10" height="10" data-glyph="${glyph}"></canvas>${name}</span>`).join('')}</div><div class="legendrow">${colors.map(([name,color])=>`<span class="legenditem"><span class="key" style="background:${color}"></span>${name}</span>`).join('')}</div>`;
  for(const el of legend.querySelectorAll('canvas')){const g=el.getContext('2d');g.strokeStyle='#b9c7cc';g.fillStyle='#65d6d2';g.lineWidth=1;drawGlyph(g,5,5,2.3,el.dataset.glyph);}}

const fieldDefs={
  sex:{label:'Gender marker',type:'select',values:['Man','Woman','Non-binary']},
  profile:{label:'Routine profile',type:'select',values:profileDefs.map(x=>x.name)},
  job:{label:'Occupation',type:'select',values:[...new Set(Object.values(jobNames).flat())]},
  activity:{label:'Current activity',type:'select',values:activities},
  district:{label:'Home district',type:'select',values:['A','B','C','D']},
  wakeHour:{label:'Wake-up time window',type:'time'},
  workStart:{label:'Work / class start window',type:'time'},
  breakfast:{label:'Breakfast habit',type:'select',values:['No breakfast','At home','Cafe','Bakery','Workplace']},
  commute:{label:'Commute mode',type:'select',values:['Walk','Bus','Metro','Bicycle','Car']},
  workSite:{label:'Work / study site',type:'select',values:workSites},
  street:{label:'Has walked on street',type:'select',values:streets},
  relation:{label:'Knows someone as',type:'select',values:['family','friend','coworker','neighbour','rival']},
  social:{label:'Current social state',type:'select',values:['In conversation','Pursuing someone','Fleeing','No active interaction']},
  event:{label:'Has encountered event',type:'select',values:['Evacuation alarm','Heavy rain','Traffic disruption','Street dispute']},
  age:{label:'Age at least',type:'number',min:16,max:100,suffix:' years'},
  household:{label:'Household size at least',type:'number',min:1,max:5,suffix:' people'},
  workDays:{label:'Work / school days at least',type:'number',min:0,max:7,suffix:' days'},
  workLength:{label:'Shift length at least',type:'number',min:0,max:16,suffix:' hours'},
  network:{label:'Known people at least',type:'number',min:0,max:100,suffix:' people'},
  streetCount:{label:'Distinct streets visited at least',type:'number',min:0,max:13,suffix:' streets'},
  streetNow:{label:'Currently on street',type:'select',values:streets},
  stress:{label:'Stress at least',type:'number',min:0,max:100,suffix:'%'}
};
function initFilters(){const select=document.getElementById('filterField');select.innerHTML=Object.entries(fieldDefs).map(([k,v])=>`<option value="${k}">${v.label}</option>`).join('');select.onchange=renderFilterValue;document.getElementById('addFilter').onclick=addFilter;document.getElementById('clearFilters').onclick=()=>{filters=[];applyFilters();renderFilterRules();};document.getElementById('filterLogic').onchange=applyFilters;renderFilterValue();}
function renderFilterValue(){const key=document.getElementById('filterField').value,def=fieldDefs[key],wrap=document.getElementById('filterValueWrap');wrap.innerHTML=`<div class="fieldtop"><span>${def.label}</span></div>`;if(def.type==='select'){const s=document.createElement('select');s.className='select';s.id='filterInput';s.innerHTML=def.values.map(x=>`<option value="${x}">${x}</option>`).join('');wrap.appendChild(s);}else if(def.type==='time'){wrap.innerHTML+=`<div style="display:flex;gap:6px"><label class="sub">from <input class="select" id="filterFrom" type="number" min="0" max="23.75" step=".25" value="7"></label><label class="sub">to <input class="select" id="filterTo" type="number" min="0" max="24" step=".25" value="9"></label></div>`;}else{wrap.innerHTML+=`<input class="select" id="filterInput" type="number" min="${def.min}" max="${def.max}" value="50">`;}}
function readFilterValue(key){const def=fieldDefs[key];if(def.type==='time')return [+document.getElementById('filterFrom').value,+document.getElementById('filterTo').value];return document.getElementById('filterInput').value;}
function addFilter(){const field=document.getElementById('filterField').value;filters.push({field,value:readFilterValue(field)});renderFilterRules();applyFilters();draw();}
function formatRule(rule){const d=fieldDefs[rule.field],value=Array.isArray(rule.value)?`${fmtHour(rule.value[0])}–${fmtHour(rule.value[1]%24)}`:rule.value;return `${d.label}: ${value}${rule.field==='stress'?'%':''}`;}
function renderFilterRules(){const wrap=document.getElementById('filterRules');wrap.innerHTML=filters.map((f,i)=>`<div class="feedrow" style="align-items:center"><span style="flex:1">${formatRule(f)}</span><button class="close" style="font-size:14px" data-remove="${i}">×</button></div>`).join('');for(const b of wrap.querySelectorAll('[data-remove]'))b.onclick=()=>{filters.splice(+b.dataset.remove,1);renderFilterRules();applyFilters();draw();};}
function hourInWindow(h,range){const [from,to]=range;return from<=to?h>=from&&h<=to:h>=from||h<=to;}
function matchesRule(a,r){switch(r.field){
 case 'sex':return a.sex===r.value;case 'profile':return a.profileName===r.value;case 'job':return a.job===r.value;case 'activity':return a.activity===r.value;case 'district':return a.district===r.value;
 case 'wakeHour':return hourInWindow(a.wakeHour,r.value);case 'workStart':return a.workdays.length>0&&hourInWindow(a.workStart,r.value);
 case 'breakfast':return r.value==='No breakfast'?!a.hasBreakfast:a.hasBreakfast&&a.breakfastVenue===(r.value==='At home'?'Home':r.value);
 case 'commute':return a.mode===r.value;case 'workSite':return a.workSite===r.value;case 'street':return a.streetVisits.has(r.value);
 case 'relation':return [...a.relationships.values()].some(x=>x.kind===r.value);
 case 'social':{const s=getSession(a);return r.value==='In conversation'?!!s&&s.kind==='conversation':r.value==='Pursuing someone'?!!s&&s.kind==='pursuit'&&s.a===a.id:r.value==='Fleeing'?!!s&&s.kind==='pursuit'&&s.b===a.id:!s;}
 case 'event':return a.eventHistory.has(r.value);case 'age':return a.age>=+r.value;case 'household':return a.householdSize>=+r.value;
 case 'workDays':return a.workdays.length>=+r.value;case 'workLength':return a.workLength>=+r.value;case 'network':return a.relationships.size>=+r.value;
 case 'streetCount':return a.streetVisits.size>=+r.value;case 'streetNow':return a.lastStreet===r.value;case 'stress':return a.stress*100>=+r.value;default:return true;}}
function applyFilters(){matchingIds.clear();const logic=document.getElementById('filterLogic')?.value||'all';for(const a of agents){const ok=filters.length===0||(logic==='all'?filters.every(r=>matchesRule(a,r)):filters.some(r=>matchesRule(a,r)));if(ok)matchingIds.add(a.id);}const text=filters.length?`${matchingIds.size.toLocaleString()} / ${agents.length.toLocaleString()}`:`All ${agents.length.toLocaleString()}`;const mc=document.getElementById('matchCount');if(mc)mc.textContent=text;const mt=document.getElementById('matchTag');if(mt)mt.textContent=`${matchingIds.size.toLocaleString()} match`;}
function updateFilterOptions(){if(document.getElementById('filterField'))renderFilterValue();}

function updateUi(){
  if(!agents.length)return;const counts=Object.fromEntries(activities.map(a=>[a,0]));let stress=0,affected=0;
  for(const a of agents){counts[a.activity]=(counts[a.activity]||0)+1;stress+=a.stress;if(events.some(e=>Math.hypot(a.x-e.x,a.y-e.y)<e.radius))affected++;}
  const avg=stress/agents.length;document.getElementById('clock').textContent=fmtHour(hour);document.getElementById('timeline').style.setProperty('--progress',`${hour/24*100}%`);document.getElementById('populationTag').textContent=`${agents.length.toLocaleString()} residents`;document.getElementById('eventTag').textContent=events.length?`${events.length} active disruption${events.length>1?'s':''}`:'No active disruptions';document.getElementById('activeStat').textContent=agents.length.toLocaleString();document.getElementById('stressStat').innerHTML=`${(avg*100).toFixed(1)}<span style="font-size:12px;color:var(--muted)">%</span>`;document.getElementById('stressHint').textContent=avg>.5?'elevated citywide':avg>.28?'rising after disruption':'within normal range';
  const days=['MONDAY','TUESDAY','WEDNESDAY','THURSDAY','FRIDAY','SATURDAY','SUNDAY'];document.getElementById('weekday').textContent=days[weekDay];document.getElementById('dayNumber').textContent=`DAY ${String(worldDay).padStart(3,'0')}`;
  const sorted=[...stepTimes].sort((x,y)=>x-y),p50=sorted.length?sorted[Math.floor((sorted.length-1)*.5)]:0,p95=sorted.length?sorted[Math.floor((sorted.length-1)*.95)]:0;document.getElementById('tickStat').innerHTML=`${p50.toFixed(2)}<span style="font-size:12px;color:var(--muted)"> ms</span>`;document.getElementById('p95').textContent=p95.toFixed(2);document.getElementById('memStat').innerHTML=`${(agents.length*352/1e6).toFixed(1)}<span style="font-size:12px;color:var(--muted)"> MB</span>`;document.getElementById('affected').textContent=affected.toLocaleString();document.getElementById('eventCount').textContent=`${events.length} active`;
  const topActivities=['Working','Commuting','Home','Sleeping','Talking','Pursuing','Fleeing','Evacuating'];document.getElementById('activityRows').innerHTML=topActivities.map(a=>`<div style="display:flex;align-items:center;gap:8px;margin:7px 0;font-size:10px"><span style="width:72px;color:#abb8c3">${a}</span><div style="height:4px;background:#29343c;flex:1;border-radius:4px"><div style="width:${counts[a]/agents.length*100}%;height:100%;background:${activityColors[a]};border-radius:4px"></div></div><span style="width:30px;text-align:right;color:#8998a8">${Math.round(counts[a]/agents.length*100)}%</span></div>`).join('');document.getElementById('weatherTag').textContent=events.some(e=>e.type==='Heavy rain')?'☂ Heavy rain':'☀ Clear';
  applyFilters();if(selected!==null)showAgent(agents[selected]);
}
function showAgent(a){if(!a)return;selected=a.id;document.getElementById('agentCard').style.display='block';document.getElementById('agentName').textContent=`Resident ${String(a.id+1).padStart(5,'0')} · ${a.job}`;
  const ns=[['Energy',a.energy],['Hunger',a.hunger],['Stress',a.stress],['Social need',a.socialNeed],['Bladder',a.bladder],['Health',a.health],['Comfort',a.comfort],['Fatigue',a.fatigue],['Fear',a.fear],['Curiosity',a.curiosity],['Money',a.money],['Risk aversion',a.riskAversion]];
  document.getElementById('needGrid').innerHTML=ns.map(([n,v])=>`<div class="need">${n}<span style="float:right;color:#afc0ca">${Math.round(v*100)}%</span><div class="track"><div class="fill" style="width:${v*100}%;background:${n==='Stress'&&v>.55?'var(--orange)':'var(--cyan)'}"></div></div></div>`).join('');
  const rel=[...a.relationships.values()].reduce((o,r)=>(o[r.kind]=(o[r.kind]||0)+1,o),{});const s=getSession(a);document.getElementById('agentMeta').textContent=`${a.sex} · age ${a.age} · ${a.profileName} · ${a.job} at ${a.workSite} · home district ${a.district}. Wakes ${fmtHour(a.wakeHour)}; breakfast ${a.hasBreakfast?`${a.breakfastVenue} at ${fmtHour(a.breakfastHour)}`:'usually skipped'}; shift ${fmtHour(a.workStart)}–${fmtHour(a.workEnd)}; ${a.mode}. Last street: ${a.lastStreet}. Streets visited: ${a.streetVisits.size}. Known ties: ${Object.entries(rel).map(([k,v])=>`${k} ${v}`).join(', ')||'none'}. Current interaction: ${s?s.kind:'none'}.`;
}
function closeAgent(){selected=null;document.getElementById('agentCard').style.display='none';draw();}
function exportData(){const visible=agents.filter(a=>matchingIds.has(a.id));const rows=['id,sex,profile,age,job,work_site,work_start,wake_time,breakfast,breakfast_place,commute,activity,district,last_street,streets_visited,known_people,stress,energy,hunger,social_need,interaction',...visible.map(a=>[a.id,a.sex,a.profileName,a.age,a.job,a.workSite,fmtHour(a.workStart),fmtHour(a.wakeHour),a.hasBreakfast?'yes':'no',a.breakfastVenue,a.mode,a.activity,a.district,a.lastStreet,[...a.streetVisits].join('|'),a.relationships.size,a.stress.toFixed(3),a.energy.toFixed(3),a.hunger.toFixed(3),a.socialNeed.toFixed(3),getSession(a)?.kind||'none'].join(','))];const blob=new Blob([rows.join('\n')],{type:'text/csv'}),url=URL.createObjectURL(blob),link=document.createElement('a');link.href=url;link.download='resident-query.csv';link.click();URL.revokeObjectURL(url);}

canvas.addEventListener('click',e=>{const rect=canvas.getBoundingClientRect(),x=(e.clientX-rect.left)/rect.width*(canvas.width/2.2),y=(e.clientY-rect.top)/rect.height*(canvas.height/2.2);if(armedEvent){inject(armedEvent,clamp(x,5,420),clamp(y,5,295));return;}let best=null,d=1e9;for(const a of agents){const q=(a.x-x)**2+(a.y-y)**2;if(q<d){d=q;best=a;}}if(best&&d<100)showAgent(best);else closeAgent();});
function togglePlay(){running=!running;const btn=document.getElementById('play');btn.textContent=running?'❚❚':'▶';btn.title=running?'Pause (Space)':'Play (Space)';document.getElementById('statusText').textContent=running?'Simulation running':'Simulation paused';document.getElementById('statusDot').style.background=running?'var(--lime)':'var(--orange)';document.getElementById('stepBtn').disabled=running;last=performance.now();}
document.getElementById('play').onclick=togglePlay;document.getElementById('stepBtn').onclick=stepOnce;
function setSpeed(v){speed=v;document.getElementById('speed').value=v;document.getElementById('speedOut').textContent=`${v}×`;for(const b of document.querySelectorAll('[data-speed]'))b.classList.toggle('active',+b.dataset.speed===v);}
for(const b of document.querySelectorAll('[data-speed]'))b.onclick=()=>setSpeed(+b.dataset.speed);
document.addEventListener('keydown',e=>{if(e.target.matches('input,select,textarea'))return;if(e.code==='Space'){e.preventDefault();togglePlay();}else if(e.key==='s'||e.key==='S'||e.key==='ArrowRight'){stepOnce();}else if(e.key==='r'||e.key==='R'){resetSimulation();}else if(e.key==='Escape'){if(armedEvent){armedEvent=null;document.getElementById('armedHint').textContent='';canvas.classList.remove('armed');}else closeAgent();}});
function timedStep(dt){const t0=performance.now();step(dt);stepTimes.push(performance.now()-t0);if(stepTimes.length>240)stepTimes.shift();}
function stepOnce(){if(!running){timedStep(16);draw();updateUi();}}
document.getElementById('population').oninput=e=>{pop=+e.target.value;document.getElementById('popOut').textContent=pop.toLocaleString();};document.getElementById('population').onchange=()=>resetSimulation();
document.getElementById('speed').oninput=e=>setSpeed(+e.target.value);
document.getElementById('seed').oninput=e=>document.getElementById('seedOut').textContent=e.target.value;document.getElementById('seed').onchange=()=>resetSimulation();
document.getElementById('colorMode').onchange=()=>{updateLegend();draw();};document.getElementById('symbolMode').onchange=()=>{updateLegend();draw();};
function resetSimulation(){const seed=+document.getElementById('seed').value;pop=+document.getElementById('population').value;rnd=makeRandom(seed);events=[];stepTimes.length=0;tick=0;hour=7.7;weekDay=1;worldDay=18;selected=null;closeAgent();document.getElementById('seedFoot').textContent=seed;createAgents(pop);document.getElementById('feed').innerHTML='<div style="color:#72818d;padding:5px 0">No events recorded. Trigger an event to observe the response.</div>';updateUi();draw();}
function loop(now){const dt=Math.min(60,now-last);last=now;if(running){timedStep(dt);draw();}if(now-lastUi>450){updateUi();lastUi=now;}requestAnimationFrame(loop);}

document.getElementById('filterLogic').value='all';initFilters();createAgents(pop);updateUi();updateLegend();draw();requestAnimationFrame(loop);
