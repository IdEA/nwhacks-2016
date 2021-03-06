import React             from 'react';
import Mozaik            from 'mozaik/browser';
import githubComponents  from 'mozaik-ext-github';
import travisComponents  from 'mozaik-ext-travis';
import timeComponents    from 'mozaik-ext-time';
import weatherComponents from 'mozaik-ext-weather';
import esp8266Components from 'mozaik-ext-esp8266';


const MozaikComponent = Mozaik.Component.Mozaik;
const ConfigActions   = Mozaik.Actions.Config;


Mozaik.Registry.addExtensions({
    github:  githubComponents,
    travis:  travisComponents,
    time:    timeComponents,
    weather: weatherComponents,
	esp: 	 esp8266Components
});

React.render(<MozaikComponent/>, document.getElementById('mozaik'));

ConfigActions.loadConfig();
