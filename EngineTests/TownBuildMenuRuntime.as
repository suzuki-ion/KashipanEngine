// Inject into CityController in the isolated runtime QA application.
 void QABuildMenu(){
  SetSettingBool("town.buildDetails",true);buildMenu.SetVisible(true);buildMenu.Select(5);buildMenu.Animate(.3f);
  QAAssert(buildMenu.DetailsDisplayed()&&buildMenu.detailShown,"build details initially visible");
  QAAssert(buildMenu.details.objects[0].GetTransform().GetTranslate().x>900,"build details are on the right");
  int active=0;for(uint i=0;i<buildMenu.catalog.length();++i)if(buildMenu.iconObjects[i*5+2].IsActive())++active;
  QAAssert(active>0&&active<=6,"only visible category icons active");
  buildMenu.Animate(.016f);QAAssert(!buildMenu.rows[0].IsActive(),"inactive category remains hidden on stable frames");
  buildMenu.ToggleDetails();QAAssert(!buildMenu.DetailsDisplayed()&&!buildMenu.detailShown&&buildMenu.selected==5&&buildMenu.rows[5].IsActive(),"hiding details preserves selection and list");
  buildMenu.Animate(.016f);QAAssert(!buildMenu.details.objects[0].IsActive(),"details remain hidden after animation update");
  buildMenu.ToggleDetails();QAAssert(buildMenu.DetailsDisplayed()&&buildMenu.details.objects[0].IsActive(),"details can be restored");
  buildMenu.Render(false);QAAssert(!buildMenu.details.objects[0].IsActive()&&!buildMenu.iconObjects[5*5].IsActive(),"modal hides preview rendering");
  buildMenu.Animate(.016f);QAAssert(buildMenu.details.objects[0].IsActive()&&buildMenu.rows[5].IsActive()&&!buildMenu.rows[0].IsActive(),"menu restores filtered layout after modal");
  buildMenu.SetVisible(false);buildMenu.Animate(.3f);QAAssert(!buildMenu.rendered&&!buildMenu.details.objects[0].IsActive(),"closing disables menu and details");
  state.NewGame();QAFunds();state.Build(4,4,4);guildMenu.Open(4,4);QAAssert(guildMenu.panel.GetTransform().GetTranslate().x>900&&guildMenu.statusObject.IsActive(),"selected building details on right");
  SetSettingBool("town.buildDetails",false);guildMenu.Refresh();QAAssert(!guildMenu.statusObject.IsActive()&&guildMenu.Bottom()==245&&guildMenu.dispatchLabel.IsActive(),"collapsed selected guild keeps dispatch accessible");
  SetSettingBool("town.buildDetails",true);guildMenu.Refresh();QAAssert(guildMenu.statusObject.IsActive()&&guildMenu.Bottom()==495,"selected details can expand again");guildMenu.Show(false);
 }
